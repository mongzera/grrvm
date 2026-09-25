
# GRR ByteCode Specifications

## Data Types
GRR Bytecode Features a standard primitive datatype for variables.

* uint8 | int8
* uint16 | int16
* uint32 | int32
* float
* char

## Variables

Variables can be assigned in the ::_const_data and ::_data subroutine, which is a reserved subroutine for assigning globally available variables.

For immutable variables / constants, this is how we declare and initialize:
```
# Sample for Immutable Variables
::_const_data
  uint8 lvl 42
  char first_name "ethan", 0
  char last_name 'g','a','m','a','t'

```

For mutable variables, this is how we declare and initialize:
```
# Sample for Mutable Variables
::_data
  uint8 lvl 42
  char first_name "ethan", 0
  char last_name 'g','a','m','a','t'

```

## Subroutines
Subroutines are used to define reusable blocks of code that can be called from other parts of the program.

```
# Sample Subroutine
::_global ; this is the main subroutine that gets called first
  PUSH 5
  STORE_L 0 ; store the value 5 at local variable index 0
  PUSH 6
  STORE_L 1 ; store the value 6 at local variable index 1

  LOAD_L 0 ; load the value at local variable index 0
  LOAD_L 1 ; load the value at local variable index 1
  CALL add

  LOAD_L 0 ; load the value at local variable index 0
  LOAD_L 1 ; load the value at local variable index 1
  CALL sub
  
  HALT ; the main subroutine ends with HALT

::add ; this is a user-defined subroutine that adds two values by popping them from the stack and pushing the result back
  ADD
  RET  ; all subroutines end with RET

::sub ; this is a user-defined subroutine that subtracts two values by popping them from the stack and pushing the result back
  SUB
  RET  ; all subroutines end with RET
```

### 'CALL'
CALL is used to call a subroutine by its name.
It pushes its own Call Stack Frame so that you can declare local variables without interfering with previous call frames.

### 'RET'
RET is used to pop the top most Call Stack Frame and return from a subroutine.
It returns to the calling subroutine.

### 'HALT'
HALT is used to terminate the program.
It stops the program from running further.

## Labels
Labels are used to mark a specific location within a subroutine.
They are used with the JUMP, JZ, JNZ instruction to jump to a specific label.

```

::_global
    CALL label_sample
    HALT


::label_sample
    PUSH 5
    PUSH 6
    CMPNEQ   ; pops 5 and 6 to compare for inequality COMPARE_NOT_EQUAL | CMPNEQ which pushes 1 if not equal, 0 if equal
    JNZ .equal
    PUSH 0
    POP
    RET
    .equal:  ; this is a label called 'equal' where JNZ calls if 5 is not equal to 6.
        PUSH 1
        POP
    RET
    
```
