package src.com.mongzera.grrvm_assembler.bytecode;

import java.io.DataOutputStream;
import java.io.IOException;

public class Data {
    private final byte type;
    private final int size;
    private final byte[] payload;

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

    public Data(byte type, byte[] payload) {
        this.type = type;
        this.payload = payload;
        this.size = payload.length;
    }

    public byte getType() {
        return type;
    }

    public int getSize() {
        return size;
    }

    public byte[] getPayload() {
        return payload;
    }

    /**
     * Streams binary in exact layout format: Type (1 byte) -> Size (4 bytes) -> Actual Data Payload
     */
    public void streamBinary(DataOutputStream dos) throws IOException {
        dos.writeByte(type);
        dos.writeInt(size);
        dos.write(payload);
    }
}
