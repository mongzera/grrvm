package src.com.mongzera.grrvm_assembler.util;

import java.nio.charset.StandardCharsets;

public class SymbolHasher {
    // 32-bit FNV-1a constants matching the C implementation
    private static final int FNV1A_PRIME = 0x01000193;
    private static final int FNV1A_OFFSET_BASIS = 0x811C9DC5;

    /**
     * Computes the 32-bit FNV-1a hash of a symbol string.
     * @param symbol The native function name (e.g., "math_abs")
     * @return The 32-bit hash matching the VM's C implementation
     */
    public static int fnv1a32(String symbol) {
        int hash = FNV1A_OFFSET_BASIS;

        // Convert to UTF-8 bytes to perfectly match C's char array traversal
        byte[] bytes = symbol.getBytes(StandardCharsets.UTF_8);

        for (byte b : bytes) {
            // b & 0xFF simulates the (g_u8) unsigned cast in C
            hash ^= (b & 0xFF);
            hash *= FNV1A_PRIME;
        }

        return hash;
    }
}
