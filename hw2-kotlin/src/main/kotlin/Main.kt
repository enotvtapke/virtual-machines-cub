package cub.virtual.machines

import cub.virtual.machines.decompiler.Bytecode
import cub.virtual.machines.interpreter.Interpreter
import java.nio.file.Path

fun main() {
    {
        val bytecode = Bytecode(Path.of("test002.bc"))
        println(bytecode)
        val instructions = generateSequence { bytecode.next() }
        println(instructions.joinToString("\n"))
    }()

    val bytecode = Bytecode(Path.of("test002.bc"))
    val interpreter = Interpreter(bytecode, listOf(1, 2, 3, 4, 5))
    println(generateSequence { interpreter.interpret() }.toList())
    println(interpreter.output())
}
