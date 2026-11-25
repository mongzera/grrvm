package src.com.mongzera.grrvm_assembler.bytecode;

import src.com.mongzera.grrvm_assembler.DebugMsg;
import src.com.mongzera.grrvm_assembler.GrrError;
import src.com.mongzera.grrvm_assembler.util.DataValueParser;

public class Data {
    private final byte type;
    private final int size;
    private final String[] values;
    private final String name;
    private int[] raw_values;
    private int assignedGlobalTableSlot = -1;

    private boolean isConstant = false;

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

    /* Datatype immediates carried by PUSH_T and H_SET_TYPE must be decodable
     * by the VM (see asm_to_vm_primtype() in include/parser/bytecode_parse.h).
     * UINT64, INT64, DOUBLE and CHAR have assembler codes but no prim_type
     * counterpart, so accept only the types the VM can actually represent.
     * Data declarations keep using matchTypeStr(); the loader rejects their
     * headers itself. */
    public static byte matchRuntimeTypeStr(String typeStr){
        switch (typeStr.toUpperCase()){
            case "UINT8":
            case "UINT16":
            case "UINT32":
            case "INT8":
            case "INT16":
            case "INT32":
            case "FLOAT":
                return matchTypeStr(typeStr);
            default:
                DebugMsg.asm_error(GrrError.INVALID_DATATYPE,
                        typeStr + " is not supported by the VM (expected UINT8, UINT16, UINT32, INT8, INT16, INT32 or FLOAT)");
                break;
        }

        return 0;
    }

    public void resolve(Bytecode bytecode, boolean isConstant) {
        this.isConstant = isConstant;
        assignedGlobalTableSlot = bytecode.getCurrentTableSlot();
        bytecode.incrementCurrentTableSlot();

        this.raw_values = new int[this.values.length];
        for (int i = 0; i < this.values.length; i++) {
            this.raw_values[i] = DataValueParser.parse(this.type, this.values[i]);
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
        raw[0] = isConstant ? (0x80 | type) : type;
        raw[1] = values.length;
        System.arraycopy(raw_values, 0, raw, 2, values.length);

        return raw;
    }

    public int getAssignedGlobalTableSlot(){
        return assignedGlobalTableSlot;
    }
}
