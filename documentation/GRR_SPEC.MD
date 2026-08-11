# GRR ByteCode Specifications

# Data
## Data Types
GRR Bytecode Features a standard primitive datatype for variables.

* uint8 | int8
* uint16 | int16
* uint32 | int32
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
