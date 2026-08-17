package src.com.mongzera.grrvm_assembler;
public class DebugMsg{

    public static final String ANSI_RESET = "\u001B[0m";
    public static final String ANSI_RED = "\u001B[31m";
    public static final String ANSI_GREEN = "\u001B[32m";
    public static final String ANSI_YELLOW = "\u001B[33m";
    public static final String ANSI_YELLOW_BACKGROUND = "\u001B[43m";

    public static void asm_info(String title, String msg){
        System.out.println(String.format("%s[%s]%s %s", ANSI_YELLOW, title, ANSI_RESET, msg));
    }

    public static void asm_error(GrrError error){
        System.out.printf(ANSI_RED + "[%s]%s %s%n", error.getTitle(),ANSI_RESET, error.getDescription());
        System.exit(error.getCode());
    }

    public static void asm_error(GrrError error, String additional){
        System.out.printf(ANSI_RED + "[%s]%s %s Additional: %s%n", error.getTitle(), ANSI_RESET, error.getDescription(), additional);
        System.exit(error.getCode());
    }

    public static void asm_error(String title, String msg){
        System.out.printf(ANSI_RED + "[%s]%s %s%n", title, ANSI_RESET, msg);
    }
}
