package cub.virtual.machines

import cub.virtual.machines.decompiler.Bytecode
import cub.virtual.machines.decompiler.decompile
import java.nio.file.Path

fun main() {
    val bytecode = Bytecode(Path.of("test001.bc"))
    println(bytecode)

    val instructions = generateSequence { decompile(bytecode) }
    println(instructions.joinToString("\n"))
}
