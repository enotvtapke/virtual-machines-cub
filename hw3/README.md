## Instruction Sequence Analyzer
Run main.cpp to start the program. Program takes a file path with bytecode as an argument.

### Output
The program prints offsets of the basic blocks.  
Then it prints the statistics for each idiom of length 1 and 2 in the following format:  
`Number of idiom occurrences` : `idiom`

### Example
The first 20 most frequent idioms from Sort.bc:
```
40 :	DROP
35 :	DUP
25 :	ELEM
18 :	CONST 1
15 :	CONST 0
15 :	CONST 1 | ELEM
14 :	DROP | DUP
13 :	DUP | CONST 1
12 :	DROP | DROP
10 :	CONST 0 | ELEM
10 :	DUP | CONST 0
9 :	ELEM | DROP
8 :	LD A 0
6 :	END
5 :	DUP | DUP
4 :	ELEM | ST L 0
4 :	JMP 0x0000039d
4 :	LD L 0
4 :	SEXP cons 2
4 :	ST L 0
...
```