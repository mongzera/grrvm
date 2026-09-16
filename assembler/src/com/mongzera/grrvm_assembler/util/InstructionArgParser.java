package src.com.mongzera.grrvm_assembler.util;

import src.com.mongzera.grrvm_assembler.DebugMsg;
import src.com.mongzera.grrvm_assembler.GrrError;

public class InstructionArgParser {

    public static int parse(String val) {
        if (val == null || val.trim().isEmpty()) {
            DebugMsg.asm_error(GrrError.INVALID_OPCODE_ARGUMENT);
            return 0; // Return early to prevent execution on invalid/null input
        }
        val = val.trim();

        // 1. Character literal check (e.g., 'a', '\n')
        if (val.length() >= 2 && val.startsWith("'") && val.endsWith("'")) {
            return parseChar(val);
        }

        String lower = val.toLowerCase();

        // 2. Hex / Binary check FIRST (Must run before float check, as hex like 0x1E contains 'e')
        if (lower.startsWith("0x") || lower.startsWith("-0x") ||
                lower.startsWith("0b") || lower.startsWith("-0b")) {
            return parseInteger(val);
        }

        // 3. Floating-point check (contains decimal point or scientific exponent notation)
        if (val.contains(".") || lower.contains("e")) {
            return parseFloat(val);
        }

        // 4. Fallback to Integer parsing (Octal or Decimal)
        return parseInteger(val);
    }

    private static int parseInteger(String val) {
        String s = val.toLowerCase();
        boolean negative = s.startsWith("-");
        if (negative) {
            s = s.substring(1);
        }

        long parsedVal;
        try {
            if (s.startsWith("0x")) {
                parsedVal = Long.parseLong(s.substring(2), 16);
            } else if (s.startsWith("0b")) {
                parsedVal = Long.parseLong(s.substring(2), 2);
            } else if (s.startsWith("0o")) {
                parsedVal = Long.parseLong(s.substring(2), 8);
            } else if (s.startsWith("0") && s.length() > 1 && s.matches("^0[0-7]+$")) {
                parsedVal = Long.parseLong(s.substring(1), 8);
            } else {
                parsedVal = Long.parseLong(s);
            }
        } catch (NumberFormatException e) {
            DebugMsg.asm_error(GrrError.INVALID_DATATYPE, "Invalid integer literal: " + val);
            return 0;
        }

        if (negative) {
            parsedVal = -parsedVal;
        }

        // --- 32-BIT SCOPE CHECK ---
        // Allows signed 32-bit [-2147483648, 2147483647] and unsigned 32-bit [0, 4294967295]
        if (parsedVal < Integer.MIN_VALUE || parsedVal > 0xFFFFFFFFL) {
            DebugMsg.asm_error(GrrError.INVALID_DATATYPE, "32-bit overflow for value: " + val);
            return 0;
        }

        return (int) parsedVal;
    }

    private static int parseFloat(String val) {
        try {
            float f = Float.parseFloat(val);
            // Converts float into IEEE 754 32-bit bit pattern
            return Float.floatToIntBits(f);
        } catch (NumberFormatException e) {
            DebugMsg.asm_error(GrrError.INVALID_DATATYPE, "Invalid float literal: " + val);
            return 0;
        }
    }

    private static int parseChar(String val) {
        String content = val.substring(1, val.length() - 1);

        if (content.length() == 1) {
            return content.charAt(0);
        }

        // Escape sequence mapping
        if (content.startsWith("\\") && content.length() == 2) {
            switch (content.charAt(1)) {
                case 'n': return '\n';
                case 'r': return '\r';
                case 't': return '\t';
                case '0': return '\0';
                case '\\': return '\\';
                case '\'': return '\'';
                case '\"': return '\"';
            }
        }

        return parseInteger(content);
    }
}