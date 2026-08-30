package src.com.mongzera.grrvm_assembler.util;

import src.com.mongzera.grrvm_assembler.DebugMsg;
import src.com.mongzera.grrvm_assembler.GrrError;

public class InstructionArgParser {

    public static int parse(String val) {
        if (val == null || val.trim().isEmpty()) {
            DebugMsg.asm_error(GrrError.INVALID_OPCODE_ARGUMENT);
        }
        val = val.trim();

        // 1. Character literal check (e.g., 'a', '\n')
        if (val.startsWith("'") && val.endsWith("'")) {
            return parseChar(val);
        }

        // 2. Floating-point check (contains decimal point or exponent notation like 1.5, .5, 1e10)
        if (val.contains(".") || val.toLowerCase().contains("e")) {
            return parseFloat(val);
        }

        // 3. Fallback to Integer parsing (Hex, Binary, Octal, or Decimal)
        return parseInteger(val);
    }

    private static int parseInteger(String val) {
        String s = val.toLowerCase();
        long parsedVal;

        if (s.startsWith("0x")) {
            parsedVal = Long.parseLong(s.substring(2), 16);
        } else if (s.startsWith("0b")) {
            parsedVal = Long.parseLong(s.substring(2), 2);
        } else if (s.startsWith("0") && s.length() > 1 && !s.contains(".")) {
            parsedVal = Long.parseLong(s.substring(1), 8);
        } else {
            parsedVal = Long.parseLong(s);
        }

        // --- 32-BIT SCOPE CHECK ---
        // For unsigned 32-bit: parsedVal < 0 || parsedVal > 0xFFFFFFFFL
        // For signed 32-bit: parsedVal < Integer.MIN_VALUE || parsedVal > Integer.MAX_VALUE
        if (parsedVal < Integer.MIN_VALUE || parsedVal > 0xFFFFFFFFL) DebugMsg.asm_error(GrrError.INVALID_DATATYPE, "32-bit overflow, 64-bit not supported!");

        return (int) parsedVal;
    }

    private static int parseFloat(String val) {
        float f = Float.parseFloat(val);
        // Converts the float into IEEE 754 floating-point 32-bit bit pattern
        return Float.floatToIntBits(f);
    }

    private static int parseChar(String val) {
        // Strip surrounding single quotes
        if (val.startsWith("'") && val.endsWith("'") && val.length() >= 3) {
            val = val.substring(1, val.length() - 1);
        }

        if (val.length() == 1) {
            return val.charAt(0);
        }

        // Escape sequence mapping
        if (val.startsWith("\\") && val.length() == 2) {
            switch (val.charAt(1)) {
                case 'n': return '\n';
                case 'r': return '\r';
                case 't': return '\t';
                case '0': return '\0';
                case '\\': return '\\';
                case '\'': return '\'';
                case '\"': return '\"';
            }
        }

        return parseInteger(val); // Fallback if given numeric ASCII directly
    }
}