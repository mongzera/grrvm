import src.com.mongzera.grrvm_assembler.BytecodeLoader;
import src.com.mongzera.grrvm_assembler.bytecode.Bytecode;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.net.URISyntaxException;
import java.net.URL;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public class Test1 {
    public static void main(String[] args) throws URISyntaxException, IOException {
        URL dir = Test1.class.getResource("/res/asm_test1.grr");

        if (dir == null) throw new FileNotFoundException("Resource not found: /res/asm_test1.grr");

        Path path = Paths.get(dir.toURI());

        // Option 1: Append .dump.txt to the full filename (asm_test1.grr.dump.txt)
        Path newPath = path.resolveSibling(path.getFileName().toString() + ".dump.txt");

        // Option 2: Replace existing extension (asm_test1.dump.txt)
        String fileName = path.getFileName().toString();
        int lastDot = fileName.lastIndexOf('.');
        String baseName = (lastDot == -1) ? fileName : fileName.substring(0, lastDot);
        Path newPathReplace = path.resolveSibling(baseName + ".dump.txt");

        Bytecode bytecode = BytecodeLoader.fromPath(path);

        assert bytecode != null;
        Files.writeString(newPath, bytecode.createBytecodeDumpFile());

        System.out.println("Original path: " + path.toAbsolutePath());
        System.out.println("Writing to: " + newPath.toAbsolutePath());
    }
}
