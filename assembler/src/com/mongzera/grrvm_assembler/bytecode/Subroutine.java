package src.com.mongzera.grrvm_assembler.bytecode;

import src.com.mongzera.grrvm_assembler.DebugMsg;

public class Subroutine extends Segment{

    public Subroutine(Bytecode bytecode, String name){
        super(bytecode, name);
    }

	@Override
	public void feed(String line) {
	    lines.add(line);
	}

	@Override
	public void compile() {
	    DebugMsg.asm_info("COMPILER", "Compiling Subroutine: " + name);
	}

	@Override
	public void resolve() {
	    DebugMsg.asm_info("RESOLVER", "Resolving Subroutine: " + name);
	}
}
