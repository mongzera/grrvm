package src.com.mongzera.grrvm_assembler.bytecode;

import src.com.mongzera.grrvm_assembler.DebugMsg;
import src.com.mongzera.grrvm_assembler.GrrError;
import src.com.mongzera.grrvm_assembler.OpCode;

import java.util.ArrayList;
import java.util.HashMap;

public class Subroutine extends Segment{
	private int subroutineOffset;
	private int subroutineSize;
	private final ArrayList<OpCode.Instruction> instructions = new ArrayList<>();
	private final HashMap<String, Integer> labelMaps = new HashMap<>();

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

		for(int i = 0; i < instructions.size(); i++){
			instructions.get(i).compile();
		}
	}

	@Override
	public void resolve() {
	    DebugMsg.asm_info("RESOLVER", "Resolving Subroutine: " + name);

		for(int i = 0; i < instructions.size(); i++){
			instructions.get(i).resolve(bytecode, this);
		}

		// check if Subroutine ends with RET or HALT (if ::_global)
		boolean closedScope = false;

		if(instructions.isEmpty()){
			DebugMsg.asm_error(GrrError.SUBROUTINE_IS_EMPTY, String.format("Subroutine [%s]", name));
		}

		if(name.equals(Bytecode.GLOBAL_SUBROUTINE)) {
			if(instructions.getLast().getOpCode().matchStr("HALT")) closedScope = true;
		}else{
			if(instructions.getLast().getOpCode().matchStr("RET")) closedScope = true;
		}

		if(!closedScope){
			DebugMsg.asm_error(GrrError.SUBROUTINE_NOT_CLOSED, String.format("Subroutine [%s]", name));
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
		if(line.startsWith(".") && line.endsWith(":")){
			parseLabel(line);
			return;
		}

		String[] tokens = line.split(" ");
		OpCode match = bytecode.getIsa().findMatch(tokens[0]);
		if(match == null) DebugMsg.asm_error(GrrError.INVALID_OPCODE, tokens[0]);

		assert match != null;
		instructions.add(match.parseInstruction(bytecode, tokens));
	}

	private void parseLabel(String line){
		if(labelMaps.containsKey(line)){
			DebugMsg.asm_error("SUBROUTINE", String.format("Duplicate label [%s] in subroutine [%s]", line, name));
		}
		String label = line.substring(0, line.length()-1); // removes : at the end of the string
		labelMaps.put(label, bytecode.getCurrentInstructionLine());
	}

	public ArrayList<OpCode.Instruction> getInstructions(){
		return instructions;
	}

	public int getSubroutineOffset(){
		return subroutineOffset;
	}

	public int findLabelAddress(String label){
		if(!labelMaps.containsKey(label)) DebugMsg.asm_error("RESOLVER", String.format("Label [%s] is undefined in the subroutine [%s]", label, name));

		return labelMaps.get(label);
	}

}
