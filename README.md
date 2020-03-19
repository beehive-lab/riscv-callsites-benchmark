# Call-site implementations on RISC-V

This microbenchmark generates an inline sequence of call-sites (in assembly) to
evaluate the overhead of different implementations.

## Call-site implementations

The microbenchmark currently supports the following call-site implementations

### Direct

The simplest implementation comprising of a single jump instruction.
This is expected to be the most efficient implementation but comes with a
limited target range.

``` assembly
JALR ra, zero, TARGET ; ±2KiB target range
```

or

``` assembly
JAL  ra, TARGET ; ±1MiB target range
```

or

``` assembly
AUIPC t1, #0xABCDE000 ; ±4GiB target range
JALR  ra, t1, #0xF98
```

Although the last approach supports the longest range, it is (probably) not patchable,
since we need to atomically patch two consequent instructions.

### Absolute-load Indirect Branching

Indirect branching allows us to jump anywhere but requires us to load the target
address in a register.
A simple approach is to hold a lookup-table in the memory that maps methods to
target addresses. 
Then at each call-site we can load the target address from that lookup table in
some register and jump to the target. 
Unfortunately the construction of an absolute address (64bit value) is probably slow. 

``` assembly
LUI   t1, #0x02137000
ADDIW t1, #0x654
SLLI  t1, t1, #12
ADDI  t1, t1, #0xEF8
SLLI  t1, t1, #12
ADDI  t1, t1, #0x9AB
SLLI  t1, t1, #8
ADDI  t1, t1, #0xCD
LD    t1, t1
JALR  ra, t1
```

### Relative-load Indirect Branching

A more promising alternative to absolute-loads is that of relative-loads.
Relative-loads enable us to load a value from a relative offset with a couple of
instructions.
This way we can keep a lookup table mapping methods to target addresses in the
header of each compiled method and quickly load the targets for indirect branching.

``` assembly
CALLEE_1 : .quad 0 x0123456789ABCDEF
           ...
CALLEE_N : .quad 0 x01234ABCDEF56789
START :    ...
           AUIPC t1, %hi(CALLEE_1)
           LD    t1, t1, %lo(CALLEE_1)
           JALR  ra, t1
```

### Trampolines

Trampolines are a smart way to use direct branches whenever possible and fall
back to relative-load indirect branching only when necessary.
This technique is employed by the ARMv8 OpenJDK implementation.
However it is expected to be less successful on RISC-V due to the more limited
direct branches (+-1MiB vs +-128MiB). 

``` assembly
L:       AUIPC t1, %hi(CALLEE)
         LD    t1, t1, %lo(CALLEE)
         JALR  zero, t1 ; Don 't link
CALLEE : .quad 0 x01234ABCDEF56789
START :  ...
         JAL   ra, SHORT_TARGET ; or L
```

## Known Issues

* A fence.i needs to be executed on each HART to observe a code patch! We
  currently don't measure this.
* We don't support partial patching of >50% of the call-sites (unless it is 100%)
* Warm up runs with patching enabled will essentially patch the to-be-patched call-sites to target either CALLEE_0 or PATCHER. As a result, measurement patched will essentially just re-write these values, not affecting the branch predictor! 
* direct with 100% patching does not work
