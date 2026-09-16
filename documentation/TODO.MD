# Ideas

## PUSH_T
Implement PUSH_T <type> <value> - because using PUSH only is lacking, cannot specify the type of data, meaning, treatment will be wrong
Example: PUSH_T uint32 123423

[Program]
0|PUSH_T
1|uint32
2|123423

## PUSH_ADDR
Implement PUSH_ADDR <mem_address> - so that we can access global variables and load it into the stack
Example: PUSH_ADDR 2

[Program]
0|PUSH_ADDR
1|2

[Stack]
[push data from address = 2, copied into the stack]
