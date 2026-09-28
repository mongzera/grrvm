
# Ideas & Todos

## PUSH_T [DONE]
Implement PUSH_T <type> <value> - because using PUSH only is lacking, cannot specify the type of data, meaning, treatment will be wrong
Example: PUSH_T uint32 123423

[Program]
0|PUSH_T
1|uint32
2|123423

## PUSH_ADDR [DONE]
Implement PUSH_ADDR <mem_address> - so that we can access global variables and load it into the stack
Example: PUSH_ADDR 2

[Program]
0|PUSH_ADDR
1|2

[Stack]
[push data from address = 2, copied into the stack]

## POP_ADDR 
Implement POP_ADDR <mem_address> - so that we can store the popped value back into a memory address
Example: POP_ADDR 2

[Program]
0|POP_ADDR
1|2

Also, Since RAM is now considered the HEAP, maybe we can separate the GLOBAL and HEAP memory

## H_SET_TYPE [DONE]
Implement H_SET_TYPE <type> - so that we can set the type of a memory address
Example: H_SET_TYPE uint32

Functionality:
Pops a value from the stack (TYPE_REFERENCE)
Then, it will target the TYPE_LENGTH it points to where it will set the type for that array.
