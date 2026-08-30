package src.com.mongzera.grrvm_assembler;

import src.com.mongzera.grrvm_assembler.bytecode.Bytecode;
import src.com.mongzera.grrvm_assembler.util.InstructionArgParser;

public class OpCode {
    private byte opcodeCategory = -1;
    private byte opcodeIndex = -1;
    private byte opcodeArgCount = 0;
    private String opcodeStrForm = "";
    private int opcodeIdForm = -1;
    private ResolverCallback resolverCallback = null;

    public OpCode(byte opcodeCategory, byte opcodeIndex, byte opcodeArgCount, String opcodeStrForm, ResolverCallback resolverCallback){
        this.init(opcodeCategory, opcodeIndex, opcodeArgCount, opcodeStrForm, resolverCallback);
    }

    public OpCode(byte opcodeCategory, byte opcodeIndex, byte opcodeArgCount, String opcodeStrForm){
        this.init(opcodeCategory, opcodeIndex, opcodeArgCount, opcodeStrForm, null);
    }

    private void init(byte opcodeCategory, byte opcodeIndex, byte opcodeArgCount, String opcodeStrForm, ResolverCallback resolverCallback){
        this.opcodeCategory = opcodeCategory;
        this.opcodeIndex = opcodeIndex;
        this.opcodeArgCount = opcodeArgCount;
        this.opcodeStrForm = opcodeStrForm.trim().toUpperCase();
        this.opcodeIdForm = (byte)(opcodeCategory | opcodeIndex);
        this.resolverCallback = resolverCallback;
    }

    public boolean matchStr(String val){
        return opcodeStrForm.equals(val.toUpperCase());
    }

    public int getOpcodeIdForm(){
        return opcodeIdForm;
    }

    public byte getArgCount(){
        return opcodeArgCount;
    }

    public byte getCategory(){
        return opcodeCategory;
    }

    public Instruction parseInstruction(Bytecode bytecode, String[] tokens){
        Instruction instruction = new Instruction(this, bytecode);
        instruction.matchArgs(tokens);

        return instruction;
    }

    public String getStrForm(){
        return opcodeStrForm;
    }



    public class Instruction{
        private String[] inTextArg;
        private int[] resolvedArgs;
        private int instructionLineNumber;
        private OpCode opCode;

        public Instruction(OpCode opCode, Bytecode bytecode){
            this.inTextArg = new String[opCode.getArgCount()];
            this.resolvedArgs = new int[opCode.getArgCount()];
            this.instructionLineNumber = bytecode.getCurrentInstructionLine();
            this.opCode = opCode;
            bytecode.addInstructionLine(opCode.getArgCount()+1);
        }

        public String getArg(int idx){
            if(opcodeArgCount > idx && idx >= 0) return inTextArg[idx];

            return "";
        }

        public void setArg(int idx, String value){
            if(opcodeArgCount > idx && idx >= 0) inTextArg[idx] = value;
        }

        public void matchArgs(String[] tokens){
            //skip tokens[0] since this is the OPCODE str
            if(tokens.length - 1 != opcodeArgCount) DebugMsg.asm_error(GrrError.ARGUMENT_COUNT_NOT_MATCH);

            for(int i = 0; i < opcodeArgCount; i++){
                setArg(i, tokens[i + 1]);
            }
        }

        public void resolve(Bytecode bytecode){
            if(opCode.resolverCallback != null) opCode.resolverCallback.resolve(bytecode, this);
        }

        public void compile(){
            for(int i = 0; i < inTextArg.length; i++){
                resolvedArgs[i] = InstructionArgParser.parse(inTextArg[i]);
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

        public int[] asStream(){
            int[] stream = new int[1 + inTextArg.length];
            stream[0] = opCode.getOpcodeIdForm();
            for(int i = 0; i < resolvedArgs.length; i++){
                stream[i+1] = resolvedArgs[i];
            }

            return stream;
        }
    }

    public interface ResolverCallback{
        void resolve(Bytecode bytecode, Instruction instruction);
    }
}
