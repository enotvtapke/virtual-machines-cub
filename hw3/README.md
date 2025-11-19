## Instruction Sequence Analyzer
Run main.cpp to start the program. Program takes a file path with bytecode as an argument.

### Output
The program prints offsets of the basic blocks.  
Then it prints the statistics for each idiom of length 1 and 2 in the following format:  
`Number of idiom occurrences` : `idiom`

### Example
The first 20 most frequent idioms from Sort.bc:
```
25 :	DUP
25 :	DROP
16 :	ELEM
13 :	CONST	0
11 :	CONST	1
9 :	DROP | DUP
8 :	LD	A(0)
8 :	DUP | CONST	0
8 :	CONST	0 | ELEM
8 :	CONST	1 | ELEM
7 :	DUP | CONST	1
6 :	END
6 :	DROP | DROP
5 :	ELEM | DROP
4 :	DUP | DUP
3 :	LD	L(1)
3 :	TAG	cons 2
3 :	CALL	0x00000202 1
3 :	BINOP	==
3 :	DUP | TAG	cons 2
...
```