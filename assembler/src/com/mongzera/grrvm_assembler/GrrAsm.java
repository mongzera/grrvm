package src.com.mongzera.grrvm_assembler;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.*;

import src.com.mongzera.grrvm_assembler.bytecode.Bytecode;

public class GrrAsm {
    public static void main(String[] args) {
        DebugMsg.OFF();
        if (args.length == 0) {
            System.err.println("Usage: grrasm <file[.grr]>");
            System.exit(1);
        }

        try {
            String inputPathStr = args[0];

            if (inputPathStr.startsWith("~" + File.separator)) {
                inputPathStr = System.getProperty("user.home") + inputPathStr.substring(1);
            }

            Path path = Paths.get(inputPathStr);
            if (!Files.exists(path) && !inputPathStr.endsWith(".grr")) {
                path = Paths.get(inputPathStr + ".grr");
            }

            if (!Files.exists(path)) {
                System.err.println("Error: File not found - " + path.toAbsolutePath());
                System.exit(1);
            }

            Bytecode bytecode = BytecodeLoader.fromPath(path);
            if (bytecode == null) {
                System.err.println("Error: Failed to load bytecode.");
                System.exit(1);
            }

            Bytecode.Stream stream = bytecode.getCompiledStream();
            int[] binaryData = stream.getStream();

            // Convert int[] to 4-byte words (Little-Endian for standard x86/ARM Linux)
            ByteBuffer buffer = ByteBuffer.allocate(binaryData.length * 4);
            buffer.order(ByteOrder.LITTLE_ENDIAN); // Match standard C uint32_t layout

            for (int word : binaryData) {
                buffer.putInt(word);
            }

            // Write raw 32-bit aligned byte stream to System.out
            System.out.write(buffer.array());
            System.out.flush();

        } catch (Exception e) {
            System.err.println("Error during assembly: " + e.getMessage());
            System.exit(1);
        }
    }
}