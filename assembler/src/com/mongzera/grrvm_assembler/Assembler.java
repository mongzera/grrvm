package src.com.mongzera.grrvm_assembler;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.stream.Stream;

import src.com.mongzera.grrvm_assembler.bytecode.Bytecode;


public class Assembler{

    public static void main(String[] args){
        // if(args.length < 2){
        //     DebugMsg.asm_error(GrrError.FILE_NOT_GIVEN);
        // }
        String currentWorkingDir = System.getProperty("user.dir");

        Path path = Path.of(currentWorkingDir, args[0]);
        Bytecode bytecode = BytecodeLoader.fromPath(path);

    }
}
