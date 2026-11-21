#!/usr/bin/env python3
"""Analyse a GRRVM telemetry dump.

Reads the CSV emitted by `INVOKE_NATIVE telemetry_flush` (from a file or
stdin), verifies its CRC32 trailer, and reports the three SOP variables:

  external fragmentation  Ef = 1 - longest / (buddy_free + slab_free)
  full heap fragmentation Ef_full = 1 - longest_free_region / free_slots,
                          measured from block headers across the whole heap
                          (algorithm-independent; see full_heap_scan)
  GC jitter               pause distribution: n, mean, sd, min, max, p50, p99
  allocation speed        elapsed_us / allocs per timed batch

Standard library only, so it runs anywhere the VM's tooling runs.

Usage:
    grrvm -r prog.grr.o | tools/analyze_telemetry.py
    tools/analyze_telemetry.py dump.csv --json
    tools/analyze_telemetry.py dump.csv --strict   # non-zero exit on CRC fail

Exit codes: 0 success, 1 CRC mismatch under --strict, 2 no telemetry found.
"""

import argparse
import json
import math
import re
import sys
import zlib
from typing import Dict, List, Optional, Tuple

HEADER_RE = re.compile(r"^#GRRTELEM,(\d+),(.*)$")
END_RE = re.compile(r"^#GRRTELEM,END,(.*)$")
KV_RE = re.compile(r"([A-Za-z_]+),([^,]+)")


def kv_pairs(text: str) -> Dict[str, str]:
    return {m.group(1): m.group(2) for m in KV_RE.finditer(text)}


def parse(text: str) -> Tuple[Dict, List[Dict]]:
    """Split a dump into metadata (params, trailer, CRC verdict) and rows."""
    params: Dict[str, str] = {}
    bench: Dict[str, str] = {}
    rows: List[Dict] = []
    trailer: Optional[str] = None
    payload_lines: List[str] = []
    saw_header = False

    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue

        if line.startswith("#GRRBENCH"):
            # "#GRRBENCH,batches,30,per_batch,64,...": strict alternating
            # key,value tokens after the tag.
            toks = line.split(",")[1:]
            bench = dict(zip(toks[0::2], toks[1::2]))
            payload_lines.append(line)
            continue

        m = END_RE.match(line)
        if m:
            trailer = m.group(1)
            break  # nothing after the trailer belongs to the payload

        if line.startswith("#GRRTELEM"):
            m = HEADER_RE.match(line)
            if m:
                params = kv_pairs(m.group(2))
                saw_header = True
            payload_lines.append(line)
            continue

        parts = line.split(",")
        kind = parts[0]
        if kind not in ("PAUSE", "FRAG", "ALLOC", "FULLSCAN"):
            continue

        try:
            if kind == "PAUSE":
                rows.append({"kind": kind, "seq": int(parts[1]),
                             "delta_us": int(parts[2])})
            elif kind == "FRAG":
                rows.append({
                    "kind": kind, "seq": int(parts[1]),
                    "pre_buddy_free": int(parts[2]),
                    "pre_slab_free": int(parts[3]),
                    "pre_longest": int(parts[4]),
                    "post_buddy_free": int(parts[5]),
                    "post_slab_free": int(parts[6]),
                    "post_longest": int(parts[7]),
                })
            elif kind == "FULLSCAN":
                rows.append({
                    "kind": kind, "seq": int(parts[1]),
                    "total_slots": int(parts[2]),
                    "allocated_slots": int(parts[3]),
                    "free_slots": int(parts[4]),
                    "free_regions": int(parts[5]),
                    "longest_free_region": int(parts[6]),
                })
            else:  # ALLOC
                rows.append({"kind": kind, "seq": int(parts[1]),
                             "allocs": int(parts[2]),
                             "elapsed_us": int(parts[3])})
        except (IndexError, ValueError):
            continue  # truncated line: the CRC check will report the loss

        payload_lines.append(line)

    crc_ok = False
    crc_expected: Optional[str] = None
    crc_actual: Optional[str] = None
    if trailer is not None:
        fields = kv_pairs(trailer)
        crc_expected = fields.get("crc")
        blob = ("\n".join(payload_lines) + "\n").encode()
        crc_actual = "0x%08X" % (zlib.crc32(blob) & 0xFFFFFFFF)
        crc_ok = (crc_expected or "").upper() == crc_actual.upper()

    meta = {"params": params, "bench": bench, "trailer": trailer,
            "crc_expected": crc_expected, "crc_actual": crc_actual,
            "crc_ok": crc_ok, "has_header": saw_header}
    return meta, rows


def stats(values: List[float]) -> Dict[str, float]:
    n = len(values)
    if n == 0:
        return {"n": 0}
    ordered = sorted(values)
    mean = sum(ordered) / n
    sd = math.sqrt(sum((v - mean) ** 2 for v in ordered) / (n - 1)) if n > 1 else 0.0

    def pct(p: float) -> float:
        # Nearest-rank percentile: smallest value at or above p% of samples.
        rank = max(1, math.ceil(p / 100.0 * n))
        return ordered[rank - 1]

    return {"n": n, "mean": mean, "sd": sd, "min": ordered[0],
            "max": ordered[-1], "p50": pct(50), "p99": pct(99)}


def ef(longest: float, free: float) -> Optional[float]:
    """Ef = 1 - longest/total_free. None when the heap has no free slots:
    a full heap has no meaningful fragmentation figure (the VM emits NaN)."""
    if free <= 0:
        return None
    return 1.0 - (longest / free)


def dropped(trailer: Optional[str], key: str) -> Optional[Tuple[int, int]]:
    """(recorded, total) for one stream from the trailer.

    The trailer repeats "<key>,<recorded>,<total>", so it is split
    positionally rather than through kv_pairs() (which keeps one value).
    """
    if trailer is None:
        return None

    parts = trailer.split(",")
    for i, part in enumerate(parts):
        if part == key and i + 2 < len(parts):
            try:
                return int(parts[i + 1]), int(parts[i + 2])
            except ValueError:
                return None
    return None


def analyse(rows: List[Dict]) -> Dict:
    pauses = [r["delta_us"] for r in rows if r["kind"] == "PAUSE"]

    frag_rows = []
    for r in rows:
        if r["kind"] != "FRAG":
            continue
        pre_free = r["pre_buddy_free"] + r["pre_slab_free"]
        post_free = r["post_buddy_free"] + r["post_slab_free"]
        frag_rows.append({
            "seq": r["seq"],
            "pre_free": pre_free,
            "pre_longest": r["pre_longest"],
            "post_free": post_free,
            "post_longest": r["post_longest"],
            "ef_pre": ef(r["pre_longest"], pre_free),
            "ef_post": ef(r["post_longest"], post_free),
        })

    fullscan_rows = []
    for r in rows:
        if r["kind"] != "FULLSCAN":
            continue
        fullscan_rows.append({
            "seq": r["seq"],
            "total_slots": r["total_slots"],
            "allocated_slots": r["allocated_slots"],
            "free_slots": r["free_slots"],
            "free_regions": r["free_regions"],
            "longest_free_region": r["longest_free_region"],
            "ef": ef(r["longest_free_region"], r["free_slots"]),
        })

    speeds = []
    for r in rows:
        if r["kind"] != "ALLOC" or r["allocs"] <= 0:
            continue
        speeds.append(r["elapsed_us"] / r["allocs"])

    ef_pre = [f["ef_pre"] for f in frag_rows if f["ef_pre"] is not None]
    ef_post = [f["ef_post"] for f in frag_rows if f["ef_post"] is not None]
    ef_full = [f["ef"] for f in fullscan_rows if f["ef"] is not None]

    return {
        "pause_us": stats(pauses),
        "ef_pre": stats(ef_pre),
        "ef_post": stats(ef_post),
        "ef_rows": frag_rows,
        "ef_full_heap": stats(ef_full),
        "fullscan_rows": fullscan_rows,
        "alloc_us_per_call": stats(speeds),
        "alloc_batches": len(speeds),
    }


def report(meta: Dict, result: Dict) -> None:
    params = meta["params"]
    print("== GRRVM telemetry ==")
    if meta["has_header"]:
        print("heap_slots : %s" % params.get("heap_slots", "?"))
    else:
        print("WARNING: no #GRRTELEM header (old-style dump); CRC unavailable")

    if meta["trailer"]:
        verdict = "OK" if meta["crc_ok"] else "MISMATCH"
        print("crc        : %s (expected %s, computed %s)"
              % (verdict, meta["crc_expected"], meta["crc_actual"]))
        for key in ("pause", "frag", "alloc", "fullscan"):
            d = dropped(meta["trailer"], key)
            if d:
                recorded, total = d
                note = "" if recorded == total else "  <-- %d DROPPED" % (total - recorded)
                print("%-10s : %d/%d recorded%s" % (key, recorded, total, note))
    if meta["bench"]:
        print("bench      : %s" % ", ".join("%s=%s" % kv for kv in meta["bench"].items()))

    p = result["pause_us"]
    if p["n"]:
        print("\n-- GC pause (jitter), us --")
        print("n=%d mean=%.1f sd=%.1f min=%d p50=%.1f p99=%.1f max=%d"
              % (p["n"], p["mean"], p["sd"], p["min"], p["p50"], p["p99"], p["max"]))

        # Static bound: marking is bounded by the thread stacks, sweeping by
        # VM_HEAP_SLOTS. Report the observed max against the sample's own
        # spread so a violation of the bound stands out.
        if p["max"] > 10 * max(p["p50"], 1):
            print("NOTE: max pause is >10x p50 - inspect that outlier before "
                  "quoting a bound")

    for label, key in (("before GC", "ef_pre"), ("after GC", "ef_post")):
        s = result[key]
        if s["n"]:
            print("\n-- External fragmentation %s --" % label)
            print("n=%d mean=%.3f min=%.3f max=%.3f"
                  % (s["n"], s["mean"], s["min"], s["max"]))

    s = result["ef_full_heap"]
    if s["n"]:
        print("\n-- Full heap fragmentation (algorithm-independent) --")
        print("n=%d mean=%.3f min=%.3f max=%.3f"
              % (s["n"], s["mean"], s["min"], s["max"]))
        for f in result["fullscan_rows"]:
            shape = ("n/a" if f["ef"] is None else "%.3f" % f["ef"])
            print("  seq=%d free=%d/%d regions=%d longest=%d ef=%s"
                  % (f["seq"], f["free_slots"], f["total_slots"],
                     f["free_regions"], f["longest_free_region"], shape))

    a = result["alloc_us_per_call"]
    if a["n"]:
        print("\n-- Allocation speed --")
        print("batches=%d us/alloc mean=%.3f sd=%.3f min=%.3f max=%.3f -> %.0f allocs/s"
              % (a["n"], a["mean"], a["sd"], a["min"], a["max"],
                 1e6 / a["mean"] if a["mean"] else 0))

    if not (p["n"] or result["ef_pre"]["n"] or a["n"]
            or result["ef_full_heap"]["n"]):
        print("\n(no samples - was a telemetry flag enabled before the workload?)")


def main() -> int:
    ap = argparse.ArgumentParser(description="Analyse a GRRVM telemetry dump")
    ap.add_argument("path", nargs="?", help="dump file (default: stdin)")
    ap.add_argument("--json", action="store_true", help="emit JSON instead of text")
    ap.add_argument("--strict", action="store_true",
                    help="exit non-zero when the CRC does not match")
    args = ap.parse_args()

    if args.path:
        with open(args.path, "r", encoding="utf-8", errors="replace") as f:
            text = f.read()
    else:
        text = sys.stdin.read()

    meta, rows = parse(text)
    if not rows and not meta["has_header"]:
        print("no GRRVM telemetry in input", file=sys.stderr)
        return 2

    result = analyse(rows)

    if args.json:
        print(json.dumps({"meta": meta, "result": result}, indent=2))
    else:
        report(meta, result)

    if args.strict and meta["trailer"] and not meta["crc_ok"]:
        print("\nCRC mismatch: the dump is corrupt or truncated", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
