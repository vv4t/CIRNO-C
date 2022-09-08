# CIRNO-C

A toy compiler/interpreter which implements its own C-like language with
modifications for an easier implementation. An example of this is fixed size
data types such as i32 or i8 rather than int or short which could vary in size
based on the compiler. An explicit function keyword fn was also used to
distinguish between global declarations and functions for easier parsing.

Furthermore, I implemented its own instruction set and virtual machine. This
was to avoid the daunting task of fully learning x86 or other such CISC
instruction, which usually listed hundreds of different operations with their
own binary format.

For the virtual machine, a stack machine instruction set was devised as, again,
it would be easier to implement. Using a stack machine meant that I could avoid
having register allocation and could simply dump the expressions on top of each
other then apply the operation.

## BUILD INSTRUCTIONS

Compilation

`make`

Examples

`make examples`

## USAGE
```
cirno [-dD] file
  d: debug
  D: dump binary
```

NOTE: The actual grammar of the language is not well documented, nor the
virtual machine or instruction set. This is because I will likely make an
improved version in the future.
