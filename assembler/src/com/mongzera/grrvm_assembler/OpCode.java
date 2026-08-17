package src.com.mongzera.grrvm_assembler;

import src.com.mongzera.grrvm_assembler.bytecode.Bytecode;

public class OpCode {
    private byte opcodeCategory = -1;
    private byte opcodeIndex = -1;
    private byte opcodeArgCount = 0;
    private String opcodeStrForm = "";
    private int opcodeIdForm = -1;

    public OpCode(byte opcodeCategory, byte opcodeIndex, byte opcodeArgCount, String opcodeStrForm){
        this.opcodeCategory = opcodeCategory;
        this.opcodeIndex = opcodeIndex;
        this.opcodeArgCount = opcodeArgCount;
        this.opcodeStrForm = opcodeStrForm.trim().toUpperCase();
        this.opcodeIdForm = (byte)(opcodeCategory | opcodeIndex);

    }

    public boolean matchStr(String val){
        return opcodeStrForm.equals(val.toUpperCase());
    }

    public boolean matchId(byte id){
        return opcodeIdForm == id;
    }

    public byte getArgCount(){
        return opcodeArgCount;
    }

    public byte getCategory(){
        return opcodeCategory;
    }

    public Instruction resolveInstruction(Bytecode bytecode, String[] tokens){
        Instruction instruction = new Instruction(this, bytecode);
        instruction.matchArgs(tokens);

        return instruction;
    }

    public String getStrForm(){
        return opcodeStrForm;
    }



    public class Instruction{
        private String[] inTextArg;
        private int instructionLineNumber;
        private OpCode opCode;

        public Instruction(OpCode opCode, Bytecode bytecode){
            this.inTextArg = new String[opCode.getArgCount()];
            this.instructionLineNumber = bytecode.getCurrentInstructionLine();
            this.opCode = opCode;
            bytecode.addInstructionLine(opCode.getArgCount()+1);
        }

        public String getArg(int idx){
            if(opcodeArgCount > idx && idx >= 0) return inTextArg[idx];

            return "";
        }

        public void matchArgs(String[] tokens){
            //skip tokens[0] since this is the OPCODE str
            if(tokens.length - 1 != opcodeArgCount) DebugMsg.asm_error(GrrError.ARGUMENT_COUNT_NOT_MATCH);

            for(int i = 0; i < opcodeArgCount; i++){
                inTextArg[i] = tokens[i + 1];
            }
        }

        public int getInstructionLineNumber(){return instructionLineNumber;}

        @Override
        public String toString(){
            StringBuilder line = new StringBuilder(opCode.getStrForm());
            for(int i = 0; i < inTextArg.length; i++){
                line.append(" ").append(inTextArg[i]);
            }

            return line.toString();
        }
    }
}
