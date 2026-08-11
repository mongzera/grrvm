package src.com.mongzera.grrvm_assembler.bytecode;

import java.util.ArrayList;

public abstract class Segment{
    protected String name;
    protected boolean closed = false;
    protected ArrayList<String> lines = new ArrayList<>();
    protected Bytecode bytecode;

    public Segment(Bytecode bytecode, String name){
        this.bytecode = bytecode;
        this.name = name;
    }

    public abstract void feed(String line);
    public abstract void compile();
    public abstract void resolve();
}
