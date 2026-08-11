package src.com.mongzera.grrvm_assembler;

public enum GrrError{
    FILE_NOT_EXIST(0xE0, "PARSER", "File does not exist!"),
    FILE_NOT_GIVEN(0xE1, "PARSER", "File must be specified!"),
    FILE_CANNOT_READ(0xE2, "PARSER", "File Parsing Error!"),
    MULTIPLE_GLOBAL_SUBROUTINES(0xF0, "PARSER", "Multiple ::_global subroutine defined!"),
    NO_GLOBAL_SUBROUTINE(0xF1, "PARSER", "Main ::_global subroutine is not defined!");

    private final int code;
    private final String title;
    private final String description;

    private GrrError(int code, String title, String description) {
        this.code = code;
        this.title = title;
        this.description = description;
    }


    public int getCode() {
        return this.code;
    }

    public String getTitle() {
        return this.title;
    }

    public String getDescription() {
        return this.description;
    }
}
