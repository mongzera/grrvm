package src.com.mongzera.grrvm_assembler;
public class DebugMsg{

    public static void asm_info(String title, String msg){
        System.out.println(String.format("[%s] %s", title, msg));
    }

    public static void asm_error(GrrError error){
        System.out.println(String.format("[%s] %s", error.getTitle(), error.getDescription()));
        System.exit(error.getCode());
    }

    public static void asm_error(String title, String msg){
        System.out.println(String.format("[%s] %s", title, msg));
    }
}
