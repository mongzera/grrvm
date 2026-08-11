package src.com.mongzera.grrvm_assembler.bytecode;

import src.com.mongzera.grrvm_assembler.DebugMsg;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

public class DataSubroutine extends Segment {
    private boolean isConstant = false;
    private final List<Data> entries = new ArrayList<>();

    public DataSubroutine(Bytecode bytecode, String name, boolean isConstant) {
        super(bytecode, name);
        this.isConstant = isConstant;
    }

    @Override
    public void feed(String line) {
        line = line.trim();
        if (line.isEmpty() || line.startsWith("#") || line.startsWith("//")) return;

        lines.add(line);
    }

    @Override
    public void compile() {
        DebugMsg.asm_info("COMPILER", "Compiling Data Subroutine: " + name);

        for (String line : lines) {
            DebugMsg.asm_info(name, ">> " + line);

            String[] var = line.split(">>");
            if (var.length < 2) {
                DebugMsg.asm_error(name, "Invalid syntax line: " + line);
                continue;
            }

            String[] type_and_name = var[0].trim().split("\\s+");
            if (type_and_name.length < 2) {
                DebugMsg.asm_error(name, "Missing type or name in: " + var[0]);
                continue;
            }

            String typeStr = type_and_name[0].trim().toLowerCase();
            String varName = type_and_name[1].trim();
            String rawValues = var[1].trim();

            try {
                byte typeId = getTypeByte(typeStr);
                byte[] rawDataBytes = parseValueData(typeStr, rawValues);

                // Instantiate standalone Data object
                Data dataEntry = new Data(typeId, rawDataBytes);
                entries.add(dataEntry);

                // Stream binary payload into output bytecode
                if (bytecode != null) {
                    ByteArrayOutputStream baos = new ByteArrayOutputStream();
                    DataOutputStream dos = new DataOutputStream(baos);

                    // Stream out binary format (Type -> Size -> Payload)
                    dataEntry.streamBinary(dos);

                    dos.flush();
                    bytecode.writeBytes(baos.toByteArray());
                }

            } catch (Exception e) {
                DebugMsg.asm_error(name, "Error compiling line: '" + line + "' -> " + e.getMessage());
            }
        }
    }

    @Override
    public void resolve() {
        DebugMsg.asm_info("RESOLVER", "Resolving Data Subroutine: " + name);
    }

    public List<Data> getEntries() {
        return entries;
    }

    private byte getTypeByte(String type) {
        switch (type) {
            case "uint8":  case "u8":  return Data.TYPE_UINT8;
            case "uint16": case "u16": return Data.TYPE_UINT16;
            case "uint32": case "u32": return Data.TYPE_UINT32;
            case "uint64": case "u64": return Data.TYPE_UINT64;
            case "int8":   case "i8":  return Data.TYPE_INT8;
            case "int16":  case "i16": return Data.TYPE_INT16;
            case "int32":  case "i32": return Data.TYPE_INT32;
            case "int64":  case "i64": return Data.TYPE_INT64;
            case "float":  case "f32": return Data.TYPE_FLOAT;
            case "double": case "f64": return Data.TYPE_DOUBLE;
            case "char":   case "string": return Data.TYPE_CHAR;
            default:
                throw new IllegalArgumentException("Unknown data type: " + type);
        }
    }

    private byte[] parseValueData(String type, String rawValues) throws IOException {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(baos);

        if (type.equals("char") || type.equals("string")) {
            String cleaned = rawValues;
            if (cleaned.startsWith("\"") && cleaned.endsWith("\"")) {
                cleaned = cleaned.substring(1, cleaned.length() - 1);
            }
            dos.write(cleaned.getBytes(StandardCharsets.UTF_8));
            dos.flush();
            return baos.toByteArray();
        }

        String[] tokens = rawValues.split(",");
        for (String token : tokens) {
            String val = token.trim();
            if (val.isEmpty()) continue;

            long parsedNum = 0;
            if (val.startsWith("0x") || val.startsWith("0X")) {
                parsedNum = Long.parseUnsignedLong(val.substring(2), 16);
            } else if (!type.equals("float") && !type.equals("double")) {
                parsedNum = Long.parseLong(val);
            }

            switch (type) {
                case "uint8": case "u8": case "int8": case "i8":
                    dos.writeByte((int) (parsedNum & 0xFF));
                    break;
                case "uint16": case "u16": case "int16": case "i16":
                    dos.writeShort((int) (parsedNum & 0xFFFF));
                    break;
                case "uint32": case "u32": case "int32": case "i32":
                    dos.writeInt((int) (parsedNum & 0xFFFFFFFFL));
                    break;
                case "uint64": case "u64": case "int64": case "i64":
                    dos.writeLong(parsedNum);
                    break;
                case "float": case "f32":
                    dos.writeFloat(Float.parseFloat(val));
                    break;
                case "double": case "f64":
                    dos.writeDouble(Double.parseDouble(val));
                    break;
            }
        }

        dos.flush();
        return baos.toByteArray();
    }
}
