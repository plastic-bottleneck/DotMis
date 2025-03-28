# DotMis

> **DotMis** is a dot-style BASIC interpreter written in C. It uses a simple, minimalistic syntax with dot-prefixed commands.

## Features

- **Interactive BASIC Interpreter:** Supports stored program lines and immediate commands.
- **Dot Commands:**  
  - `.r` – Run the stored program  
  - `.ls` – List stored program lines  
  - `.new` – Clear all stored program lines  
  - `.c` – Clear the screen  
  - `.sav` – Save program to a file  
  - `.loa` – Load program from a file (trims trailing spaces)  
  - `.bep` – Beep  
  - `.wt` – Wait (in milliseconds)  
  - `//` – Comment line  
  - `.let` – Variable assignment  
  - `.p` – Print (supports printing text and variables together)  
  - `.in` – Input  
  - `.if` … `.th` – Conditional (if … then, with any command following)  
  - `.gt` – Goto  
  - `.gs` – Gosub  
  - `.rtn` – Return from subroutine  
  - `.?` – Display help menu  
  - `.q` – Quit interpreter  
  - `.e` – End the stored program  
- **Extended Numeric Range**
- **Error Reporting**
- **Command History**

## Installation

### Requirements

- readline lib

### Compilation

Clone the repository or download the source file (`dotmis.c`), then compile using the provided Makefile.

#### Using the Makefile

Run the following command in the directory containing the Makefile:

```sh
make
```
This will compile dotmis.c and produce the executable dotmis.

Alternatively, compile manually:

```
cc dotmis.c -o dotmis -lreadline -lm
```
### Usage
Launch the interpreter by running:

```
./dotmis
```
At the prompt, you can either enter dot-commands or store a BASIC program by typing a line number followed by a command. For example:

- Immediate commands:

`.r` – Runs the stored program.

`.q` – Quits the interpreter.

- Example program:
Load the following example program with `.loa fibonacci.plcb`.
It'll look like this:

```basic
10 // Fibonacci in DotMis
15 .p "Enter number of Fibonacci numbers: "
20 .in X
30 .if X < 1 .th 200
40 .let A = 1
50 .p A
60 .if X = 1 .th 200
70 .let B = 1
80 .p B
90 .let C = 2
100 .if C > X .th 200
110 .let D = A + B
120 .p D
130 .let A = B
140 .let B = D
150 .let C = C + 1
160 .if C <= X .th 110
200 .e
```
Then type .r to run the program.

### Help
Type .? at the DotMis prompt to display a help menu that lists all available commands.
