package cub.virtual.machines

import cub.virtual.machines.decompiler.Bytecode
import cub.virtual.machines.interpreter.Interpreter
import java.nio.file.Path
import kotlin.io.path.Path
import kotlin.io.path.listDirectoryEntries
import kotlin.io.path.readText

fun main() {
    test("test*.lama")
    test("Sort.lama")
}

private fun test(pattern: String) {
    val tests = Path("./regression")
    tests.listDirectoryEntries(pattern).sorted().forEach { source ->
        val filename = source.fileName.toString().removeSuffix(".lama")
        val input =
            tests.resolve(Path("$filename.input")).readText().split("\n").filter { it.isNotEmpty() }.map { it.toInt() }
        println("Testing $filename\n")

        var bytecode = Bytecode(tests.resolve(Path("$filename.bc")))
        generateSequence {
            val pos = bytecode.code.position()
            val instr = bytecode.next()
            println("$pos: $instr")
            instr
        }.toList()
        bytecode = Bytecode(tests.resolve(Path("$filename.bc")))
        val interpreter = Interpreter(bytecode, input)
        try {
            while (interpreter.interpret() != null) {
            }
        } catch (e: Exception) {
            println(bytecode)
            println(interpreter.state)
            throw e
        }
        println("\n" + interpreter.output())
        println("\n======================\n")
    }
}
