# Assembler Documentation
This will provide you an idea how to assembler works under the hood

## Datatype and Mutability

How datatype ids

|datatype|id|
|-----|-----|
|TYPE_UINT8  |0x01|
|TYPE_UINT16 |0x02|
|TYPE_UINT32 |0x03|
|TYPE_UINT64 |0x04|
|TYPE_INT8   |0x05|
|TYPE_INT16  |0x06|
|TYPE_INT32  |0x07|
|TYPE_INT64  |0x08|
|TYPE_FLOAT  |0x09|
|TYPE_DOUBLE |0x0A|
|TYPE_CHAR   |0x0B|

Mutable tag
|mutable|immutable|
|---|---|
|0|1|

When combined with the datatype, it becomes a 32bit word
|isConstant|datatype|Binary|
|----|----|----|
|yes|uint8|0b1000000000000001|
|no|uint8|0b0000000000000001|
|yes|uint16|0b1000000000000010|
|no|uint16|0b1000000000000010|

## Binary Layout
The binary layout of the executable looks like this <br>
This is a binary stream of size 32bit
|offset|segment|
|---|---|
|0|program_offset|
|1|program_size|
|2|global_start|
|3|memory_decl|
|program_start|program|
