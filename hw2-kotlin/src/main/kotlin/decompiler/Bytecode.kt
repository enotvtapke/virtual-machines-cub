package cub.virtual.machines.decompiler

import cub.virtual.machines.Designation
import cub.virtual.machines.Instruction
import cub.virtual.machines.Instruction.ARRAY
import cub.virtual.machines.Instruction.BEGIN
import cub.virtual.machines.Instruction.BINOP
import cub.virtual.machines.Instruction.CALL
import cub.virtual.machines.Instruction.CALLC
import cub.virtual.machines.Instruction.CJMP
import cub.virtual.machines.Instruction.CLOSURE
import cub.virtual.machines.Instruction.CONST
import cub.virtual.machines.Instruction.DROP
import cub.virtual.machines.Instruction.DUP
import cub.virtual.machines.Instruction.ELEM
import cub.virtual.machines.Instruction.END
import cub.virtual.machines.Instruction.FAIL
import cub.virtual.machines.Instruction.JMP
import cub.virtual.machines.Instruction.LD
import cub.virtual.machines.Instruction.LDA
import cub.virtual.machines.Instruction.LINE
import cub.virtual.machines.Instruction.PATT
import cub.virtual.machines.Instruction.RET
import cub.virtual.machines.Instruction.SEXP
import cub.virtual.machines.Instruction.ST
import cub.virtual.machines.Instruction.STA
import cub.virtual.machines.Instruction.STI
import cub.virtual.machines.Instruction.STRING
import cub.virtual.machines.Instruction.SWAP
import cub.virtual.machines.Instruction.TAG
import cub.virtual.machines.Operation
import cub.virtual.machines.Pattern
import java.nio.ByteBuffer
import java.nio.ByteOrder.LITTLE_ENDIAN
import java.nio.file.Path
import kotlin.io.path.readBytes
import kotlin.text.Charsets.UTF_8

class Bytecode {
    private val publicSymbolsNumber: Int
    private val publicTable: ByteBuffer
    private val stringTable: ByteBuffer
    private val code: ByteBuffer
    private val globalArea: ByteBuffer

    constructor(path: Path) {
        val bytes = ByteBuffer.wrap(path.readBytes()).asReadOnlyBuffer().order(LITTLE_ENDIAN)
        val stringTableSize = bytes.int
        val globalAreaSize = bytes.int
        val publicSymbolsNumber = bytes.int

        val publicTable = bytes.slice(bytes.position(), publicSymbolsNumber * 2 * Int.SIZE_BYTES).order(LITTLE_ENDIAN)
        bytes.position(bytes.position() + publicTable.capacity())

        val stringTable = bytes.slice(bytes.position(), stringTableSize).order(LITTLE_ENDIAN)
        bytes.position(bytes.position() + stringTable.capacity())

        val code = bytes.slice(bytes.position(), bytes.capacity() - bytes.position()).order(LITTLE_ENDIAN)
        val globalArea = ByteBuffer.allocate(globalAreaSize * Int.SIZE_BYTES).order(LITTLE_ENDIAN)

        this.publicSymbolsNumber = publicSymbolsNumber
        this.publicTable = publicTable
        this.stringTable = stringTable
        this.code = code
        this.globalArea = globalArea
    }

    fun offset() = code.position()

    fun jump(position: Int) {
        code.position(position)
    }

    fun next(): Instruction? {
        val byte = code.get()
        if (byte.toInt() == -1) return null
        val high = (byte.toInt() and 0xF0) shr 4
        val low = byte.toInt() and 0x0F

        @OptIn(ExperimentalStdlibApi::class)
        fun unknownOpcode(): Nothing {
            throw IllegalArgumentException("Unknown opcode ${byte.toHexString()} at offset ${code.position()}")
        }

        return when (high) {
            0 -> BINOP(Operation.entries[low])
            1 -> when (low) {
                0 -> CONST(code.int)
                1 -> STRING(string())
                2 -> SEXP(string(), code.int)
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

            2 -> LD(designation(low))
            3 -> LDA(designation(low))
            4 -> ST(designation(low))
            5 -> when (low) {
                0 -> CJMP("z", code.int)
                1 -> CJMP("nz", code.int)
                2 -> BEGIN(code.int, code.int) // without closure
                3 -> BEGIN(code.int, code.int) // with closure
                4 -> {
                    val l = code.int
                    val n = code.int
                    val list = buildList { repeat(n) { add(designation(code.get().toInt())) } }
                    CLOSURE(l, n, list)
                }

                5 -> CALLC(code.int)
                6 -> CALL(code.int, code.int)
                7 -> TAG(string(), code.int)
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

    private fun string(): String {
        val index = code.int
        stringTable.position(index)
        val bytes = mutableListOf<Byte>()
        while (true) {
            val byte = stringTable.get()
            if (byte == 0.toByte()) break
            bytes.add(byte)
        }
        stringTable.clear()
        return bytes.toByteArray().toString(UTF_8)
    }

    private fun designation(designation: Int): Designation =
        when (designation) {
            0 -> Designation.Global(code.int)
            1 -> Designation.Local(code.int)
            2 -> Designation.Arg(code.int)
            3 -> Designation.Access(code.int)
            else -> throw Exception("Unknown designation $designation")
        }

    override fun toString(): String {
        return "Bytecode(publicSymbolsNumber=$publicSymbolsNumber, publicTable=$publicTable, stringTable=$stringTable, code=$code, globalArea=$globalArea)"
    }
}
