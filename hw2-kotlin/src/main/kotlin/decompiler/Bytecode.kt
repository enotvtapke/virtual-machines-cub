package cub.virtual.machines.decompiler

import cub.virtual.machines.Designation
import java.nio.ByteBuffer
import java.nio.ByteOrder.LITTLE_ENDIAN
import java.nio.file.Path
import kotlin.io.path.readBytes
import kotlin.text.Charsets.UTF_8

data class Bytecode(
    val publicSymbolsNumber: Int,
    val publicTable: ByteBuffer,
    val stringTable: ByteBuffer,
    val code: ByteBuffer,
    val globalArea: ByteBuffer,
) {
    fun string(): String {
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

    fun designation(designation: Int): Designation =
        when (designation) {
            0 -> Designation.Global(code.int)
            1 -> Designation.Local(code.int)
            2 -> Designation.Arg(code.int)
            3 -> Designation.Access(code.int)
            else -> throw Exception("Unknown designation $designation")
        }

    companion object {
        operator fun invoke(path: Path): Bytecode {
            val bytes = ByteBuffer.wrap(path.readBytes()).asReadOnlyBuffer().order(LITTLE_ENDIAN)
            val stringTableSize = bytes.int
            val globalAreaSize = bytes.int
            val publicSymbolsNumber = bytes.int

            val publicTable =
                bytes.slice(bytes.position(), publicSymbolsNumber * 2 * Int.SIZE_BYTES).order(LITTLE_ENDIAN)
            bytes.position(bytes.position() + publicTable.capacity())

            val stringTable = bytes.slice(bytes.position(), stringTableSize).order(LITTLE_ENDIAN)
            bytes.position(bytes.position() + stringTable.capacity())

            val code = bytes.slice(bytes.position(), bytes.capacity() - bytes.position()).order(LITTLE_ENDIAN)
            val globalArea = ByteBuffer.allocate(globalAreaSize * Int.SIZE_BYTES).order(LITTLE_ENDIAN)
            return Bytecode(
                publicSymbolsNumber = publicSymbolsNumber,
                publicTable = publicTable,
                stringTable = stringTable,
                code = code,
                globalArea = globalArea,
            )
        }
    }
}

fun designation(low: Int, index: Int): Designation =
    when (low) {
        0 -> Designation.Global(index)
        1 -> Designation.Local(index)
        2 -> Designation.Arg(index)
        3 -> Designation.Access(index)
        else -> throw Exception("Unknown designation $low")
    }
