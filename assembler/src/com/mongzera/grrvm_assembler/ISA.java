package src.com.mongzera.grrvm_assembler;

import src.com.mongzera.grrvm_assembler.bytecode.Bytecode;
import src.com.mongzera.grrvm_assembler.bytecode.Subroutine;

import java.util.ArrayList;
import java.util.HashMap;

public class ISA {
    public static byte OPC_STACK_OPERAND  = (byte) 0x00;
    public static byte OPC_ARITHMETIC     = (byte) 0x10;
    public static byte OPC_CONDITIONAL    = (byte) 0x20;
    public static byte OPC_BRANCHING      = (byte) 0x30;
    public static byte OPC_BITWISE        = (byte) 0x40;
    public static byte OPC_MEMORY_LOCAL   = (byte) 0x50;
    public static byte OPC_MEMORY_GLOBAL  = (byte) 0x60;
    public static byte OPC_MEMORY_HEAP    = (byte) 0x70;
    public static byte OPC_SYS            = (byte) 0x80;
    public static byte OPC_UART           = (byte) 0x90;

    public ArrayList<OpCode> operationCodes = new ArrayList<>();

    public ISA(){

        // STACK OPERAND
        operationCodes.add(new OpCode(OPC_STACK_OPERAND, (byte) 0x00, (byte) 0, "HALT"));
        operationCodes.add(new OpCode(OPC_STACK_OPERAND, (byte) 0x01, (byte) 1, "PUSH"));
        operationCodes.add(new OpCode(OPC_STACK_OPERAND, (byte) 0x02, (byte) 0, "POP"));
        operationCodes.add(new OpCode(OPC_STACK_OPERAND, (byte) 0x03, (byte) 0, "DUP"));
        operationCodes.add(new OpCode(OPC_STACK_OPERAND, (byte) 0x04, (byte) 0, "ROT"));
        operationCodes.add(new OpCode(OPC_STACK_OPERAND, (byte) 0x05, (byte) 0, "SWAP"));

        // ARITHMETIC OPERAND
        operationCodes.add(new OpCode(OPC_ARITHMETIC , (byte) 0x00, (byte) 0, "ADD"));
        operationCodes.add(new OpCode(OPC_ARITHMETIC , (byte) 0x01, (byte) 0, "SUB"));
        operationCodes.add(new OpCode(OPC_ARITHMETIC , (byte) 0x02, (byte) 0, "MUL"));
        operationCodes.add(new OpCode(OPC_ARITHMETIC , (byte) 0x03, (byte) 0, "DIV"));
        operationCodes.add(new OpCode(OPC_ARITHMETIC , (byte) 0x04, (byte) 0, "MOD"));
        operationCodes.add(new OpCode(OPC_ARITHMETIC , (byte) 0x05, (byte) 0, "INC"));
        operationCodes.add(new OpCode(OPC_ARITHMETIC , (byte) 0x06, (byte) 0, "DEC"));

        // CONDITIONAL OPERAND
        operationCodes.add(new OpCode(OPC_CONDITIONAL, (byte) 0x00, (byte) 0, "CMPEQ"));
        operationCodes.add(new OpCode(OPC_CONDITIONAL, (byte) 0x01, (byte) 0, "CMPNEQ"));
        operationCodes.add(new OpCode(OPC_CONDITIONAL, (byte) 0x02, (byte) 0, "CMPLT"));
        operationCodes.add(new OpCode(OPC_CONDITIONAL, (byte) 0x03, (byte) 0, "CMPLTE"));
        operationCodes.add(new OpCode(OPC_CONDITIONAL, (byte) 0x04, (byte) 0, "CMPGT"));
        operationCodes.add(new OpCode(OPC_CONDITIONAL, (byte) 0x05, (byte) 0, "CMPGTE"));

        // BRANCHING OPERAND
        operationCodes.add(new OpCode(OPC_BRANCHING, (byte) 0x00, (byte) 1, "JUMP"));
        operationCodes.add(new OpCode(OPC_BRANCHING, (byte) 0x01, (byte) 1, "JZ"));
        operationCodes.add(new OpCode(OPC_BRANCHING, (byte) 0x02, (byte) 1, "JNZ"));
        operationCodes.add(new OpCode(OPC_BRANCHING, (byte) 0x03, (byte) 1, "CALL", (bytecode, instruction) -> {
            // get name of the subroutine
            String subroutine = instruction.getArg(0);
            Subroutine targetSubroutine = bytecode.findSubroutine(subroutine);

            if(targetSubroutine == null){
                DebugMsg.asm_error(GrrError.SUBROUTINE_NOT_DEFINED, subroutine);
            }

            assert targetSubroutine != null;
            instruction.setArg(0, Integer.toString(targetSubroutine.getSubroutineOffset()));

        }));
        operationCodes.add(new OpCode(OPC_BRANCHING, (byte) 0x04, (byte) 0, "RET"));

        // BITWISE OPERAND
        operationCodes.add(new OpCode (OPC_BITWISE, (byte) 0x00, (byte) 0, "AND"));
        operationCodes.add(new OpCode (OPC_BITWISE, (byte) 0x01, (byte) 0, "OR"));
        operationCodes.add(new OpCode (OPC_BITWISE, (byte) 0x02, (byte) 0, "XOR"));
        operationCodes.add(new OpCode (OPC_BITWISE, (byte) 0x03, (byte) 0, "NOT"));
        operationCodes.add(new OpCode (OPC_BITWISE, (byte) 0x04, (byte) 0, "LSHIFT"));
        operationCodes.add(new OpCode (OPC_BITWISE, (byte) 0x05, (byte) 0, "RSHIFT"));

        // MEMORY (LOCAL) OPERAND
        operationCodes.add(new OpCode(OPC_MEMORY_LOCAL , (byte) 0x00, (byte) 1, "STORE_L"));
        operationCodes.add(new OpCode(OPC_MEMORY_LOCAL , (byte) 0x01, (byte) 1, "LOAD_L"));

        // MEMORY (GLOBAL) OPERAND
        operationCodes.add(new OpCode(OPC_MEMORY_GLOBAL, (byte) 0x00, (byte) 0, "STORE"));
        operationCodes.add(new OpCode(OPC_MEMORY_GLOBAL, (byte) 0x01, (byte) 0, "LOAD"));
        operationCodes.add(new OpCode(OPC_MEMORY_GLOBAL, (byte) 0x02, (byte) 0, "LOCK"));
        operationCodes.add(new OpCode(OPC_MEMORY_GLOBAL, (byte) 0x03, (byte) 0, "UNLOCK"));

        // MEMORY (HEAP) OPERAND
        operationCodes.add(new OpCode(OPC_MEMORY_HEAP , (byte) 0x00, (byte) 0, "H_ALLOC"));
        operationCodes.add(new OpCode(OPC_MEMORY_HEAP , (byte) 0x01, (byte) 0, "H_FREE"));

    }

    public OpCode findMatch(String opcodeStrForm){
        OpCode temp = null;
        for(int i = 0; i < operationCodes.size(); i++){
            temp = operationCodes.get(i);
            if(temp.matchStr(opcodeStrForm)) return temp;
        }

        return null;
    }
}
