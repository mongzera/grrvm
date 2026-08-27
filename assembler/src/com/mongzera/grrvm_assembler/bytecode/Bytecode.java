package src.com.mongzera.grrvm_assembler.bytecode;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.time.LocalDateTime;
import java.util.ArrayList;

import src.com.mongzera.grrvm_assembler.DebugMsg;
import src.com.mongzera.grrvm_assembler.GrrError;
import src.com.mongzera.grrvm_assembler.ISA;

public class Bytecode{
    public static final int PARSE_DATA       = 0;
    public static final int PARSE_SUBROUTINE = 1;

    private boolean dataSubroutineResovled = false;
    private String filename;
    private int mode = -1;
    private Segment currentSegment = null;
    private boolean hasGlobalSubroutine = false;
    private ArrayList<Segment> segments = new ArrayList<>();
    private int currentInstructionLine = 0;
    private int currentRAMAddressAlloc = 0;

    private ISA isa;

    private final ByteArrayOutputStream stream = new ByteArrayOutputStream();

    public Bytecode(String filename){
        this.filename   = filename;
        this.isa        = new ISA();
    }

    public void setMode(int mode){
        this.mode = mode;
    }

    public void setSubroutine(String name){
        DebugMsg.asm_info("SUBROUTINE", "Creating: " + name);
        if(name.equals("_global")){
            if(hasGlobalSubroutine) DebugMsg.asm_error(GrrError.MULTIPLE_GLOBAL_SUBROUTINES);
            hasGlobalSubroutine = true;
        }

        this.currentSegment = new Subroutine(this, name);
        segments.add(currentSegment);
    }

    public void setDataSubroutine(String name){
        boolean isConstant = false;
        if(name.startsWith("_const")) isConstant = true;

        this.currentSegment = new DataSubroutine(this, name, isConstant);
        segments.add(currentSegment);
    }

    public void feed(String line){
        currentSegment.feed(line);
    }

    public Stream compile(){
        // check first if ::_global exists, main method
        if(!hasGlobalSubroutine) DebugMsg.asm_error(GrrError.NO_GLOBAL_SUBROUTINE);

        Stream stream = new Stream(segments);

        return stream;
    }


    public void resolve(){

        // resolve data segment first
        segments.forEach((segment) -> {
            if(segment instanceof DataSubroutine) segment.resolve();
        });

        dataSubroutineResovled = true;

        // then resolve subroutines
        segments.forEach((segment) -> {
            if(segment instanceof Subroutine) segment.resolve();
        });
    }

    public void parse(){
        segments.forEach(Segment::parse);
    }

    public String createBytecodeDumpFile(){
        StringBuilder dump = new StringBuilder();

        dump.append(String.format("Dumpfile for [%s]\n", filename));
        dump.append(String.format("Created at: %s\n", LocalDateTime.now()));
        dump.append("=======================================================================\n");

        for(int i = 0; i < segments.size(); i++){
            segments.get(i).printDump(dump);
        }

        return dump.toString();
    }

    public void addInstructionLine(int nLines){
        this.currentInstructionLine += nLines;
    }

    public int getCurrentInstructionLine(){return currentInstructionLine;}


    public ISA getIsa() {
        return isa;
    }

    public void setIsa(ISA isa) {
        this.isa = isa;
    }

    public Subroutine findSubroutine(String name){
        for(int i = 0; i < segments.size(); i++){
            // check if segment is a subroutine
            Segment s = segments.get(i);
            if(!(s instanceof Subroutine subroutine)) continue;

            if(subroutine.name.equals(name)) return subroutine;
        }

        return null;
    }

    public int getAbsoluteMemAddr(String dataSubroutineVar){
        if(!dataSubroutineResovled) DebugMsg.asm_error("DATA SUBROUTINE", "Data subroutines are still unresolved!");
        DebugMsg.asm_info("DATA SUBROUTINE", "Variable: " + dataSubroutineVar);

        for(int i = 0; i < segments.size(); i++){
            // check if segment is a subroutine
            Segment s = segments.get(i);
            if(s instanceof DataSubroutine){
                DataSubroutine dataSubroutine = (DataSubroutine) s;
                String[] tokens = dataSubroutineVar.split("\\.");
                if(!dataSubroutine.name.equals(tokens[0].trim())) continue;

                Data data = dataSubroutine.getVarName(tokens[1].trim());
                if(data == null) DebugMsg.asm_error("UNKNOWN VARIABLE", String.format("Variable [%s] is not defined!", dataSubroutineVar));

                assert data != null;
                return data.getAssignedRAMAddress();
            }
        }

        return -1;
    }

    public int getCurrentRAMAddress(){return currentRAMAddressAlloc;}
    public void incrementCurrrentRamAddress(){currentRAMAddressAlloc++;}

    public static class Stream{
        private int programSize;
        private int programStart;
        private int constDataSize;
        private int constDataStart;
        private int dataSize;
        private int dataStart;

        private int[] program;

        public Stream(ArrayList<Segment> segments){
            
        }
    }
}
