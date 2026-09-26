package src.com.mongzera.grrvm_assembler;

import src.com.mongzera.grrvm_assembler.bytecode.Bytecode;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.stream.Stream;

public class BytecodeLoader {
    public static Bytecode fromPath(Path path){
        Bytecode bytecode = new Bytecode(path.getFileName().toString());
        // Read File
        try (Stream<String> lines = Files.lines(path)) {
            lines.forEach((line)->{
                if(line.contains(";")){
                    int idx = line.indexOf(";");
                    line = line.substring(0, idx);

                }
                line = line.trim();

                if(line.isEmpty()) return;
                //System.out.println(line);

                if(line.startsWith(":=")) {
                    bytecode.setMode(Bytecode.PARSE_DATA);
                    bytecode.setDataSubroutine(line.substring(2).trim());
                    return;
                }

                if(line.startsWith("::")) {
                    bytecode.setMode(Bytecode.PARSE_SUBROUTINE);
                    bytecode.setSubroutine(line.substring(2).trim());
                    return;
                }

                bytecode.feed(line);
            });

            bytecode.parse();
            bytecode.resolve();
            bytecode.compile();

            return bytecode;
        } catch (IOException e) {
            e.printStackTrace();
            DebugMsg.asm_error(GrrError.FILE_CANNOT_READ);
        }

        return null;
    }
}
