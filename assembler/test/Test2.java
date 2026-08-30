import src.com.mongzera.grrvm_assembler.BytecodeLoader;
import src.com.mongzera.grrvm_assembler.bytecode.Bytecode;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.net.URISyntaxException;
import java.net.URL;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public class Test2 {
    public static void main(String[] args) throws URISyntaxException, IOException {
        URL dir = Test2.class.getResource("/res/asm_test2.grr");

        if (dir == null) throw new FileNotFoundException("Resource not found: /res/asm_test2.grr");

        Path path = Paths.get(dir.toURI());

        // Option 1: Append .dump.txt to the full filename (asm_test1.grr.dump.txt)
        Path newPath = path.resolveSibling(path.getFileName().toString() + ".dump.txt");

        Path binaryPath = path.resolveSibling(path.getFileName().toString() + ".o");



        Bytecode bytecode = BytecodeLoader.fromPath(path);
        assert bytecode != null;
        Bytecode.Stream stream = bytecode.getCompiledStream();

        assert bytecode != null;
        Files.writeString(newPath, bytecode.createBytecodeDumpFile());
        stream.writeBinaryFile(binaryPath.toString());

        System.out.println("Original path: " + path.toAbsolutePath());
        System.out.println("Writing to: " + newPath.toAbsolutePath());
    }
}
