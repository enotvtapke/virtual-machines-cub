package cub.virtual.machines

import cub.virtual.machines.decompiler.Bytecode
import cub.virtual.machines.interpreter.Interpreter
import java.nio.file.Path
import kotlin.io.path.Path
import kotlin.io.path.listDirectoryEntries
import kotlin.io.path.readText

fun main() {
//    {
//        val bytecode = Bytecode(Path.of("test001.bc"))
//        println(bytecode)
//        val instructions = generateSequence { bytecode.next() }
//        println(instructions.joinToString("\n"))
//    }()
//
//    val bytecode = Bytecode(Path.of("test002.bc"))
//    val interpreter = Interpreter(bytecode, listOf(1, 2, 3, 4, 5))
//    println(generateSequence { interpreter.interpret() }.toList())
//    println(interpreter.output())

    val tests = Path("./regression")
    println(tests.toAbsolutePath())
    tests.listDirectoryEntries("Sort.lama").sorted().forEach { source ->
        val filename = source.fileName.toString().removeSuffix(".lama")
        val input = tests.resolve(Path("$filename.input")).readText().split("\n").filter{ it.isNotEmpty() }.map { it.toInt() }
        println("Testing $filename\n")

        var bytecode = Bytecode(tests.resolve(Path("$filename.bc")))
        val instructions = generateSequence {
            val pos = bytecode.code.position()
            val instr = bytecode.next()
            println("$pos: $instr")
            instr
        }.toList()
        bytecode = Bytecode(tests.resolve(Path("$filename.bc")))
        val interpreter = Interpreter(bytecode, input)
        try {
            while (interpreter.interpret() != null) { }
//            generateSequence { interpreter.interpret() }.toList()
        } catch (e: Exception) {
            println(bytecode)
            println(interpreter.state)
            throw e
        }
        println(interpreter.output())
        println("\n======================\n")
    }
}
