#!/usr/bin/env bash

set -e
# ==============================================================================
# Compile README.md
# ==============================================================================
cat documentation/ARCHITECTURE.MD documentation/GRR_SPEC.MD documentation/ASSEMBLER.MD documentation/OPCODES.MD documentation/TYPES.MD documentation/TODO.MD > README.md
