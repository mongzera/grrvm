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
    private final ArrayList<Data> entries = new ArrayList<>();

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

    }

    @Override
    public void resolve() {
        DebugMsg.asm_info("RESOLVER", "Resolving Data Subroutine: " + name);
        entries.forEach(entry -> {
            entry.resolve(bytecode, isConstant);
        });
    }

    @Override
    public void parse() {
        DebugMsg.asm_info("PARSER", "Parsing Subroutine: " + name);
        for(int i = 0; i < lines.size(); i++){
            evaluate(lines.get(i));
        }
    }

    private void evaluate(String line){
        String[] tokens = line.trim().split("[\\s,]+");
        byte datatype = Data.matchTypeStr(tokens[0]);
        String name = tokens[1];

        String[] values = new String[tokens.length - 2];
        System.arraycopy(tokens, 2, values, 0, values.length);

        Data data = new Data(datatype, name, values);
        entries.add(data);
    }

    @Override
    public void printDump(StringBuilder dump) {
        dump.append(":=").append(name).append(String.format(" [Variable Count: %s]", entries.size())).append("\n");
        for(int i = 0; i < entries.size(); i++){
            Data entry = entries.get(i);
            int[] stream = entry.asStream();
            dump.append(String.format("|%s|%s|\n", isConstant ? "CONST" : "IMMUT", entry.getName()));
            dump.append(String.format("\t\t TYPE: %s\n", stream[0]));
            dump.append(String.format("\t\t LENGTH: %s\n", stream[1]));
            for(int j = 0; j < stream.length-2; j++){
                dump.append(String.format("\t\t\t [%s]: %s | HEX: 0x%s\n", j, stream[j+2], Integer.toHexString(stream[j+2]).toUpperCase()));
            }
        }
        dump.append("\n");
    }

    public ArrayList<Data> getEntries() {
        return entries;
    }

    public Data getVarName(String varName){
        for(int i = 0; i < entries.size(); i++){
            if(entries.get(i).getName().equals(varName)) return entries.get(i);
        }

        return null;
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

}
