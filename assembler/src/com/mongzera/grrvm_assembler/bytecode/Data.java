package src.com.mongzera.grrvm_assembler.bytecode;

import src.com.mongzera.grrvm_assembler.DebugMsg;
import src.com.mongzera.grrvm_assembler.GrrError;
import src.com.mongzera.grrvm_assembler.util.DataValueParser;

import java.io.DataOutputStream;
import java.io.IOException;

public class Data {
    private final byte type;
    private final int size;
    private final String[] values;
    private final String name;
    private int[] raw_values;
    private int assignedRAMAddress = -1;
    // Type Identifier Constants
    public static final byte TYPE_UINT8  = 0x01;
    public static final byte TYPE_UINT16 = 0x02;
    public static final byte TYPE_UINT32 = 0x03;
    public static final byte TYPE_UINT64 = 0x04;
    public static final byte TYPE_INT8   = 0x05;
    public static final byte TYPE_INT16  = 0x06;
    public static final byte TYPE_INT32  = 0x07;
    public static final byte TYPE_INT64  = 0x08;
    public static final byte TYPE_FLOAT  = 0x09;
    public static final byte TYPE_DOUBLE = 0x0A;
    public static final byte TYPE_CHAR   = 0x0B;

    public Data(byte type, String name, String[] values) {
        this.type = type;
        this.values = values;
        this.name = name;
        this.size = values.length;
    }

    public String getName(){return name;}

    public byte getType() {
        return type;
    }

    public int getSize() {
        return size;
    }

    public boolean isArray() {return values.length > 1;}

    public String[] getValuesStr() {
        return values;
    }

    // TODO: Create a method that will parse the value, 1203, 12.23, 0xFE, etc according to type

    public static byte matchTypeStr(String typeStr){
        switch (typeStr.toUpperCase()){
            case "UINT8": return TYPE_UINT8;
            case "UINT16": return TYPE_UINT16;
            case "UINT32": return TYPE_UINT32;
            case "UINT64": return TYPE_UINT64;
            case "INT8": return TYPE_INT8;
            case "INT16": return TYPE_INT16;
            case "INT32": return TYPE_INT32;
            case "INT64": return TYPE_INT64;
            case "FLOAT": return TYPE_FLOAT;
            case "DOUBLE": return TYPE_DOUBLE;
            case "CHAR": return TYPE_CHAR;
            default:
                DebugMsg.asm_error(GrrError.INVALID_DATATYPE, typeStr);
                break;
        }

        return 0;
    }

    public void resolve(Bytecode bytecode) {

        assignedRAMAddress = bytecode.getCurrentRAMAddress();
        this.raw_values = new int[this.values.length];
        for (int i = 0; i < this.values.length; i++) {
            this.raw_values[i] = DataValueParser.parse(this.type, this.values[i]);
            bytecode.incrementCurrrentRamAddress();
        }
    }

    /*
     * The stream is arranged in this manner
     * [type]
     * [length]
     * [elements...]
     */
    public int[] asStream(){
        int[] raw = new int[values.length + 2];
        raw[0] = type;
        raw[1] = values.length;
        System.arraycopy(raw_values, 0, raw, 2, values.length);

        return raw;
    }

    public int getAssignedRAMAddress(){
        return assignedRAMAddress;
    }
}
