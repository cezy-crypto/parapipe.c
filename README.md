# parapipe.c

This is my **project assignment** for implementing parallel command execution in C.

## Goal
- Understand how the OS manages processes.
- Practice using threads (`pthread`), processes (`fork`, `exec`), and pipes (`pipe`, `dup2`).
- Perform Inter-Process Communication (IPC).

## Features
- Run multiple shell commands in parallel using threads.
- Chain commands with `->`.  
  Example: `grep abc -> grep 123 -> sort -u -> wc -l`
- Input is read from **stdin**, output is printed by a receiver thread.

## Example Usage
```bash
cat largefile.txt | ./parapipe -n 4 -c "grep abc -> grep 123"

