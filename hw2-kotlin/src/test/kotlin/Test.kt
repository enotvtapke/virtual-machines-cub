import cub.virtual.machines.decompiler.Bytecode
import cub.virtual.machines.interpreter.Interpreter
import java.nio.file.Path
import kotlin.io.path.Path
import kotlin.io.path.listDirectoryEntries
import kotlin.io.path.readBytes
import kotlin.io.path.readText
import kotlin.test.Test

class TestSuite {
    @Test
    fun test() {
        val tests = Path("./regression")
        println(tests.toAbsolutePath())
        tests.listDirectoryEntries("*.lama").sorted().forEach { source ->
            val filename = source.fileName.toString().removeSuffix(".lama")
            val input = tests.resolve(Path("$filename.input")).readText().split("\n").filter{ it.isNotEmpty() }.map { it.toInt() }
            println("Testing $filename")

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
                generateSequence { interpreter.interpret() }.toList()
            } catch (e: Exception) {
                println(bytecode)
                println(interpreter.state)
                throw e
            }
            println(interpreter.output())
            println("\n======================\n")
        }
    }
}