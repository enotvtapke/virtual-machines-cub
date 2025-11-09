## Instruction Sequence Analyzer
Run main.cpp to start the program.

The analyzer works in three phases:
1) FIND_BASIC_BLOCKS – creates basic blocks.
2) CALCULATE_CFG – builds the control flow graph.
3) ANALYZE_FREQUENCIES – goes through all reachable blocks in the control flow graph and counts statistics for each instruction (of length 1 and 2).

### Output
The results are shown in descending order:
Number of instruction occurrences : Instruction
