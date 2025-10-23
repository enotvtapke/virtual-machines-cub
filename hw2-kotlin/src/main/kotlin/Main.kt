package cub.virtual.machines

import cub.virtual.machines.decompiler.Bytecode
import java.nio.file.Path

fun main() {
    val bytecode = Bytecode(Path.of("test001.bc"))
    println(bytecode)

    val instructions = generateSequence { bytecode.next() }
    println(instructions.joinToString("\n"))
}
