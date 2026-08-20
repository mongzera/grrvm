package src.com.mongzera.grrvm_assembler.bytecode;

import src.com.mongzera.grrvm_assembler.DebugMsg;
import src.com.mongzera.grrvm_assembler.GrrError;
import src.com.mongzera.grrvm_assembler.OpCode;

import java.util.ArrayList;

public class Subroutine extends Segment{
	private int subroutineOffset;
	private int subroutineSize;
	private final ArrayList<OpCode.Instruction> instructions = new ArrayList<>();
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
		// TODO:: Resolve subroutines especially with CALL and RET address that needs absolute instruction lines
		for(int i = 0; i < instructions.size(); i++){
			instructions.get(i).resolve(bytecode);
		}
	}

	@Override
	public void parse() {
		DebugMsg.asm_info("PARSER", "Parsing Subroutine: " + name);
		subroutineOffset = bytecode.getCurrentInstructionLine();
		for(int i = 0; i < lines.size(); i++){
			evaluate(lines.get(i));
		}
		subroutineSize = bytecode.getCurrentInstructionLine() - subroutineOffset;
	}

	@Override
	public void printDump(StringBuilder dump) {
		OpCode.Instruction instruction = null;
		dump.append("::").append(name).append(String.format(" [Instruction Count: %s]\n", subroutineSize));
		for(int i = 0; i < instructions.size(); i++){
			instruction = instructions.get(i);
			dump.append(String.format("%s | %s\n", instruction.getInstructionLineNumber(), instruction));
		}
		dump.append("\n");
	}

	public void evaluate(String line){
		String[] tokens = line.split(" ");
		OpCode match = bytecode.getIsa().findMatch(tokens[0]);
		if(match == null) DebugMsg.asm_error(GrrError.INVALID_OPCODE, tokens[0]);

		assert match != null;
		instructions.add(match.parseInstruction(bytecode, tokens));
	}

	public int getSubroutineOffset(){
		return subroutineOffset;
	}

}
