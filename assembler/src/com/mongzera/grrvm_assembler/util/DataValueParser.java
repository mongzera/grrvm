package src.com.mongzera.grrvm_assembler.util;


import src.com.mongzera.grrvm_assembler.DebugMsg;
import src.com.mongzera.grrvm_assembler.GrrError;
import src.com.mongzera.grrvm_assembler.bytecode.Data;

public class DataValueParser {

    public static int parse(byte type, String valStr) {
        String cleanStr = valStr.trim();

        try {
            switch (type) {
                case Data.TYPE_UINT8:
                case Data.TYPE_INT8:
                    return parseInteger(cleanStr) & 0xFF; // Zero-padded 8-bit stored in 32-bit word

                case Data.TYPE_UINT16:
                case Data.TYPE_INT16:
                    return parseInteger(cleanStr) & 0xFFFF; // Zero-padded 16-bit stored in 32-bit word

                case Data.TYPE_UINT32:
                case Data.TYPE_INT32:
                    return parseInteger(cleanStr); // Native Java 32-bit int

                case Data.TYPE_FLOAT:
                    float fVal = Float.parseFloat(cleanStr);
                    return Float.floatToRawIntBits(fVal); // IEEE 754 32-bit raw bit pattern

                case Data.TYPE_CHAR:
                    return parseChar(cleanStr) & 0xFF;

                case Data.TYPE_UINT64:
                case Data.TYPE_INT64:
                case Data.TYPE_DOUBLE:
                    DebugMsg.asm_error(GrrError.INVALID_DATATYPE, "64-bit types are not supported.");
                    return 0;

                default:
                    DebugMsg.asm_error(GrrError.INVALID_DATATYPE, "Unsupported type: " + type);
                    return 0;
            }
        } catch (Exception e) {
            DebugMsg.asm_error(GrrError.INVALID_DATATYPE, "Failed to parse value '" + valStr + "' for type " + type);
            return 0;
        }
    }

    private static int parseInteger(String val) {
        String s = val.toLowerCase();
        if (s.startsWith("0x")) {
            return (int) Long.parseLong(s.substring(2), 16);
        } else if (s.startsWith("0b")) {
            return (int) Long.parseLong(s.substring(2), 2);
        } else if (s.startsWith("0") && s.length() > 1 && !s.contains(".")) {
            return (int) Long.parseLong(s.substring(1), 8);
        }
        return (int) Long.parseLong(s);
    }

    private static int parseChar(String val) {
        // Handles single character, character literals like 'a', or escape sequences like '\n'
        if (val.startsWith("'") && val.endsWith("'") && val.length() >= 3) {
            val = val.substring(1, val.length() - 1);
        }
        if (val.length() == 1) {
            return val.charAt(0);
        }
        if (val.startsWith("\\") && val.length() == 2) {
            switch (val.charAt(1)) {
                case 'n': return '\n';
                case 'r': return '\r';
                case 't': return '\t';
                case '0': return '\0';
                case '\\': return '\\';
                case '\'': return '\'';
            }
        }
        return parseInteger(val); // Fallback if given numeric ASCII directly
    }
}