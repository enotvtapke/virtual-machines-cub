package cub.virtual.machines.decompiler

import cub.virtual.machines.decompiler.Instruction.*

@OptIn(ExperimentalStdlibApi::class)
fun decompile(bytecode: Bytecode): Instruction? {
    val code = bytecode.code
    val byte = code.get()
    if (byte.toInt() == -1) return null
    val high = (byte.toInt() and 0xF0) shr 4
    val low = byte.toInt() and 0x0F

    @OptIn(ExperimentalStdlibApi::class)
    fun unknownOpcode(): Nothing {
        throw IllegalArgumentException("Unknown opcode ${byte.toHexString()}")
    }

    return when (high) {
        0 -> BINOP(Operation.entries[low])
        1 -> when (low) {
            0 -> CONST(code.int)
            1 -> STRING(bytecode.string())
            2 -> SEXP(bytecode.string(), code.int)
            3 -> STI
            4 -> STA
            5 -> JMP(code.int)
            6 -> END
            7 -> RET
            8 -> DROP
            9 -> DUP
            10 -> SWAP
            11 -> ELEM
            else -> unknownOpcode()
        }

        2 -> LD(bytecode.designation(low))
        3 -> LDA(bytecode.designation(low))
        4 -> ST(bytecode.designation(low))
        5 -> when (low) {
            0 -> CJMP("z", code.int)
            1 -> CJMP("nz", code.int)
            2 -> BEGIN(code.int, code.int) // without closure
            3 -> BEGIN(code.int, code.int) // with closure
            4 -> {
                val l = code.int
                val n = code.int
                val list = buildList { repeat(n) { add(designation(code.get().toInt(), code.int)) } }
                CLOSURE(l, n, list)
            }

            5 -> CALLC(code.int)
            6 -> CALL(code.int, code.int)
            7 -> TAG(bytecode.string(), code.int)
            8 -> ARRAY(code.int)
            9 -> FAIL(code.int, code.int)
            10 -> LINE(code.int)
            else -> unknownOpcode()
        }

        6 -> PATT(Pattern.entries[low])
        7 -> when (low) {
            0 -> CALL(-1, -1) // Lread
            1 -> CALL(-1, -1) // Lwrite
            2 -> CALL(-1, -1) // Llength
            3 -> CALL(-1, -1) // Lstring
            4 -> CALL(-1, code.int) // Barray
            else -> unknownOpcode()
        }

        else -> unknownOpcode()
    }
}
