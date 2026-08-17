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

    private String filename;
    private int mode = -1;
    private Segment currentSegment = null;
    private boolean hasGlobalSubroutine = false;
    private ArrayList<Segment> segments = new ArrayList<>();
    private int currentInstructionLine = 0;

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

    public void compile(){
        // check first if ::_global exists, main method
        if(!hasGlobalSubroutine) DebugMsg.asm_error(GrrError.NO_GLOBAL_SUBROUTINE);

        segments.forEach((segment) -> {
            segment.compile();
        });
    }


    public void resolve(){
        segments.forEach((segment) -> {
            segment.resolve();
        });
    }

    public void parse(){
        segments.forEach((segment) -> {
            segment.parse();
        });
    }

    public String createBytecodeDumpFile(){
        StringBuilder dump = new StringBuilder();

        dump.append(String.format("Dumpfile for [%s]\n", filename));
        dump.append(String.format("Created at: %s\n", LocalDateTime.now()));
        dump.append("=======================================================================\n");

        for(int i = 0; i < segments.size(); i++){

            // only print the subroutines, not data subroutines
            if(segments.get(i) instanceof Subroutine) segments.get(i).printDump(dump);
        }

        return dump.toString();
    }

    public void addInstructionLine(int nLines){
        this.currentInstructionLine += nLines;
    }

    public int getCurrentInstructionLine(){return currentInstructionLine;}

    /**
         * Appends an array of bytes to the bytecode binary stream.
         *
         * @param bytes Byte array to write into bytecode.
         */
        public synchronized void writeBytes(byte[] bytes) {
            if (bytes == null || bytes.length == 0) return;
            try {
                stream.write(bytes);
            } catch (IOException e) {
                DebugMsg.asm_error("BYTECODE", "Failed to write bytes to stream: " + e.getMessage());
            }
        }

        /**
         * Appends a sub-slice of a byte array to the bytecode binary stream.
         */
        public synchronized void writeBytes(byte[] bytes, int off, int len) {
            if (bytes == null || len <= 0) return;
            stream.write(bytes, off, len);
        }

        /**
         * Writes a single byte into the bytecode binary stream.
         */
        public synchronized void writeByte(byte b) {
            stream.write(b & 0xFF);
        }

        /**
         * Writes a 32-bit big-endian integer into the bytecode binary stream.
         */
        public synchronized void writeInt(int value) {
            stream.write((value >>> 24) & 0xFF);
            stream.write((value >>> 16) & 0xFF);
            stream.write((value >>> 8) & 0xFF);
            stream.write(value & 0xFF);
        }

        /**
         * Retrieves the entire compiled bytecode as a raw byte array.
         */
        public byte[] getBytes() {
            return stream.toByteArray();
        }

        /**
         * Gets the total size of the compiled bytecode in bytes.
         */
        public int getByteCount() {
            return stream.size();
        }

        /**
         * Writes the bytecode directly to an output stream (e.g., FileOutputStream).
         */
        public void writeToStream(OutputStream out) throws IOException {
            stream.writeTo(out);
        }

        /**
         * Resets/clears the output bytecode buffer.
         */
        public synchronized void clear() {
            stream.reset();
        }

    public ISA getIsa() {
        return isa;
    }

    public void setIsa(ISA isa) {
        this.isa = isa;
    }
}
