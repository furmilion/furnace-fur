# Jfsantos' SCSP DSP Assembler
---
This is a small document about how does
Jfsantos' assembler works, as I didnt find
any docs for it.

## Directives

I have no idea on how else to call these.
Directives dictate which areas the code below belongs to.

There are 3 directives:
- `#COEF`: coefficients section
- `#ADRS`: addresses section
- `#PROG`: actual DSP program section

Here is an example to show how those work:
```vliw
#COEF
percent = %50
frac = 0.5
hex = &H800

#ADRS
readOff  = 0
writeOff = ms240

#PROG
NOP
```

### Coefficients and addresses

Coefficients are internally stored as a signed 13-bit integer ranging from -4096 to 4095.

`ZERO` is a reserved coefficient with the value of 0.

Coefficients can have one of the following types:
- Percentage (coefficient only): a percentage of 4096.
  Always preceded by the `%` symbol.
  Rounded to integer and caps at 100.
- Fraction: fractional multiplier of 4096.
  Follows `x.y` syntax, where `x` is whole number (0 or 1) and `y` is any fraction.
  Limited from `0.0` to `1.0`.
- Hexadecimal: raw hexadecimal value between 0 and 4096.
  Always preceded by `&H`.
  Ranges from `&H0000` to `&H1000`.
- Milliseconds (address only): raw millisecond offset.
  Always preceded by `ms`. Cannot be fractional.
  Final value is calculated using the formula `44100 * (x / 100)`, where `x` is input. The sample rate is **always** 44100.


## Program area

### Registers
When programming the DSP, following registers are available:
- `TEMPxxx`: temporary registers. `xxx` ranges from 0 to 127.
- `EFREGxx`: mixer feedback channels registers. `xx` ranges from 0 to 15.
- `FREG`: feedback register.
- `ADREG`: address register.
- `MEMSxx`: `xx` ranges from 0 to 31.
- `MIXSxx`: `xx` ranges from 0 to 31.
- `EXTSxx`:

### Operations
There are 3 kinds of operations:
- Read: no prefix.
- Math: `@ ` prefix. Can only perform addition and multiplication.
  Several operations can be cained in form of `@ A * B + (C * D +)`,
  the assembler automatically splits them into separate steps.
  The first instruction in the chain always zeroes out the accumulator first, the other ones accumulate on top.
- Write: `> ` prefix.

### Keywords
Now this is the juice.

The DSP has the memory for exactly 128 steps.
If the program has more, the excess will be cut off and a warning will be thrown.

Memory work instructions can only happen on odd steps.

Here is the list:
- `NOP`: self-explanatory. No ooeration. Used to align memory operations.
  Usually there's no need to use those, as the assembler automatically inserts those, but those can be useful as visualizers.
- `LDI MEMSxx, MR[y]`: read from memory region `y` and push read value into MEMSxx register.
- `MR MR[y]`: read from memory region `y`.
- `IW MEMSxx`: push read value into MEMSxx register.
- `MW MR[y]`: write accumulator value at memory region `y`.
- `LDY <source>`: load value into Y from source. Source can be one of the following: `MEMSxx`, `MIXSxx`, `EXTSxx`.
- `LDA <source>`: ditto, but loads into `ADREG` instead.
- `S1 <comma-separated destinations>`: write value to destinations, bitshifted left once. Destination can be one of the registers. Always clears `FREG`.
- `S2 <comma-separated destinations>`: ditto, but the value is leftshifted twice.
- `S3 <comma-separated destinations>`: ditto, but the value is leftshifted thrice.
- `=END`: marks the end of the program.
#### Memory work
There are following modifiers when working with memory (`MR[]`):
- `DEC`: sets the hardware TABLE flag to 1 if present, else 0. When present, the address is instead decremented.
- `ADREG` or `ADRS`: use address register for offset.
- `1`: next address.
- `/NF`: sets hardware `NOFLOW` flag, preventing automatic memory increment.
