package cub.virtual.machines

import cub.virtual.machines.decompiler.Bytecode
import cub.virtual.machines.interpreter.Interpreter
import java.nio.file.Path
import kotlin.io.path.Path
import kotlin.io.path.listDirectoryEntries
import kotlin.io.path.readText

fun main() {
    test("test046.lama")
//    val time = System.currentTimeMillis()
//    test("Sort.lama")
//    val duration = System.currentTimeMillis() - time
//    println("Total time for sort: ${duration / 1000.0} seconds")
}

private fun test(pattern: String, printDecompiled: Boolean = false) {
    val tests = Path("./regression")
    tests.listDirectoryEntries(pattern).sorted().forEach { source ->
        val filename = source.fileName.toString().removeSuffix(".lama")
        val input =
            tests.resolve(Path("$filename.input")).readText().split("\n").filter { it.isNotEmpty() }.map { it.toInt() }
        println("Testing $filename\n")

        var bytecode = Bytecode(tests.resolve(Path("$filename.bc")))
        if (printDecompiled) {
            println("Decompiled code:")
            generateSequence {
                val pos = bytecode.code.position()
                val instr = bytecode.next()
                println("$pos: $instr")
                instr
            }.toList()
            bytecode = Bytecode(tests.resolve(Path("$filename.bc")))
            println()
        }
        val interpreter = Interpreter(bytecode, input)
        try {
            while (interpreter.interpret() != null) {
            }
        } catch (e: Exception) {
            println(bytecode)
            println(interpreter.state)
            throw e
        }
        println("Output:\n" + interpreter.output().joinToString("\n"))
        println("\n======================\n")
    }
}
