package cub.virtual.machines.interpreter

import cub.virtual.machines.Builtin
import cub.virtual.machines.Designation
import cub.virtual.machines.Instruction
import cub.virtual.machines.Instruction.*
import cub.virtual.machines.Operation
import cub.virtual.machines.Pattern
import cub.virtual.machines.decompiler.Bytecode

class Interpreter(private val bytecode: Bytecode, input: List<Int>) {
    val state: State = State(
        controlStack = ArrayDeque(),
        opStack = ArrayDeque(),
        globals = mutableMapOf(),
        local = Local(emptyArray(), emptyArray(), emptyArray()),
        inputStream = input.toMutableList(),
        outputStream = mutableListOf()
    )

    fun output() = state.outputStream.toList()

    private fun error(instruction: Instruction, message: String): Nothing {
        throw InterpreterException(
            """Error while interpreting instruction $instruction.
                |Message: "$message".
                |Bytecode: $bytecode
                |Interpreter state: $state
            """.trimMargin()
        )
    }

    fun interpret(): Unit? {
        val instruction = bytecode.next()
        if (instruction == null) return null
        fun error(message: String): Nothing {
            error(instruction, message)
        }
        when (instruction) {
            is IMPORT, is PUBLIC, is EXTERN, is LINE -> Unit
            is BINOP -> {
                val y = state.opStack.removeLast()
                val x = state.opStack.removeLast()
                when (instruction.op) {
                    Operation.EQ -> {
                        val z = if (x is Value.IntVal && y is Value.IntVal) {
                            evalOperation(instruction.op)(x.value, y.value)
                        } else if (x is Value.IntVal || y is Value.IntVal) {
                            0
                        } else {
                            error("Cannot compare non-integer values $x and $y")
                        }

                        state.opStack.addLast(Value.IntVal(z))
                    }

                    else -> {
                        state.opStack.addLast(Value.IntVal(evalOperation(instruction.op)(x.asIntVal(), y.asIntVal())))
                    }
                }
            }

            is CONST -> state.opStack.addLast(Value.IntVal(instruction.value))
            is STRING -> state.opStack.addLast(Value.StringVal(instruction.value.toByteArray()))
            is SEXP -> state.opStack.addLast(
                Value.Sexp(
                    instruction.tag,
                    state.opStack.removeLast(instruction.n).reversed().toTypedArray()
                )
            )

            ELEM -> evalBuiltIn(Builtin.ELEM)
            is LD -> {
                when (instruction.designation) {
                    is Designation.Global -> state.globals[instruction.designation.index]
                        ?: throw IllegalArgumentException("Unknown global ${instruction.designation.index}")

                    is Designation.Local -> state.local.locals[instruction.designation.index]
                    is Designation.Access -> state.local.closure[instruction.designation.index]
                    is Designation.Arg -> state.local.args[instruction.designation.index]
                    is Designation.Fun -> error("Cannot load ${instruction.designation} on stack")
                }.also {
                    state.opStack.addLast(it)
                }
            }

            is LDA -> state.opStack.addLast(Value.Var(instruction.designation))
            is ST -> state.update(instruction.designation, state.opStack.last())
            STI -> {
                val value = state.opStack.removeLast()
                val variable = state.opStack.removeLast() as Value.Var
                state.update(variable.designation, value)
                state.opStack.addLast(value)
            }

            STA -> {
                val value = state.opStack.removeLast()
                val variable = state.opStack.removeLast()
                when (variable) {
                    is Value.Var -> state.update(variable.designation, value).also { state.opStack.addLast(value) }
                    is Value.IntVal -> {
                        val array = state.opStack.removeLast()
                        updateElem(array, variable.value, value)
                        state.opStack.addLast(value)
                    }

                    else -> error("Unknown arg for STA $variable")
                }
            }

            is SLABEL, is FLABEL, is LABEL -> Unit
            is JMP -> bytecode.jump(instruction.position)
            is CJMP -> {
                when (instruction.cond) {
                    "z" -> if (state.opStack.removeLast().asIntVal() == 0) bytecode.jump(instruction.position)
                    "nz" -> if (state.opStack.removeLast().asIntVal() != 0) bytecode.jump(instruction.position)
                    else -> error("Unknown condition ${instruction.cond}")
                }
            }

            is CLOSURE -> {
                val closure = instruction.closureArgs.map {
                    when (it) {
                        is Designation.Arg -> state.local.args[it.index]
                        is Designation.Access -> state.local.closure[it.index]
                        is Designation.Local -> state.local.locals[it.index]
                        else -> error("Wrong value $it in closure")
                    }
                }
                state.opStack.addLast(Value.Closure(instruction.offset, closure))
            }

            is CALL_BUILTIN -> evalBuiltIn(instruction.builtin)
            is CALL_ARRAY_BUILTIN -> evalBuiltIn(Builtin.ARRAY, instruction.n)
            is CALL -> {
                val args = state.opStack.removeLast(instruction.n).reversed().toTypedArray()
                state.controlStack.addLast(StackFrame(bytecode.offset(), state.local))
                state.local = Local(args, emptyArray(), emptyArray())
                bytecode.jump(instruction.l)
            }

            is CALLC -> {
                val args = state.opStack.removeLast(instruction.n) // TODO inefficient
                val f = state.opStack.removeLast()
                state.opStack.addAll(args)
                when (f) {
                    is Value.Builtin -> evalBuiltIn(f.name, instruction.n)
                    is Value.Closure -> {
                        val args = state.opStack.removeLast(instruction.n).reversed().toTypedArray()
                        state.controlStack.addLast(StackFrame(bytecode.offset(), state.local))
                        state.local = Local(args, emptyArray(), f.closure.toTypedArray())
                        bytecode.jump(f.offset)
                    }

                    else -> error("Cannot call value $f")
                }
            }

            is BEGIN -> state.local.locals = Array(instruction.localsNum) { Value.Empty }
            END, RET -> if (state.controlStack.isNotEmpty()) {
                val frame = state.controlStack.removeLast()
                state.local = frame.local
                bytecode.jump(frame.callOffset)
            } else return null

            DROP -> state.opStack.removeLast()
            DUP -> state.opStack.addLast(state.opStack.last())
            SWAP -> {
                val x = state.opStack.removeLast()
                val y = state.opStack.removeLast()
                state.opStack.addLast(x)
                state.opStack.addLast(y)
            }

            is TAG -> {
                val x = state.opStack.removeLast()
                val r = when (x) {
                    is Value.Sexp -> if (x.tag == instruction.tag && x.value.size == instruction.arity) 1 else 0
                    else -> 0
                }
                state.opStack.addLast(Value.IntVal(r))
            }

            is ARRAY -> {
                val x = state.opStack.removeLast()
                val r = when (x) {
                    is Value.Array -> if (x.value.size == instruction.size) 1 else 0
                    else -> 0
                }
                state.opStack.addLast(Value.IntVal(r))
            }

            is FAIL -> throw LamaException("matching value ${state.opStack.last()} failure at location (${instruction.line}, ${instruction.column})")
            is PATT -> {
                val x = state.opStack.removeLast()
                val r = when (instruction.pattern) {
                    Pattern.StrCmp -> {
                        val y = state.opStack.removeLast()
                        if (x is Value.StringVal && y is Value.StringVal && x.value.contentEquals(y.value)) {
                            1
                        } else {
                            0
                        }
                    }

                    Pattern.String -> if (x is Value.StringVal) 1 else 0
                    Pattern.Array -> if (x is Value.Array) 1 else 0
                    Pattern.Sexp -> if (x is Value.Sexp) 1 else 0
                    Pattern.Boxed -> if (x !is Value.IntVal) 1 else 0
                    Pattern.UnBoxed -> if (x is Value.IntVal) 1 else 0
                    Pattern.Closure -> if (x is Value.Closure) 1 else 0
                }
                state.opStack.addLast(Value.IntVal(r))
            }
        }
        return Unit
    }

    private fun evalOperation(operation: Operation): (Int, Int) -> Int {
        fun Boolean.toInt() = if (this) 1 else 0
        fun Int.toBool() = this != 0

        return when (operation) {
            Operation.ADD -> { x, y -> x + y }
            Operation.SUB -> { x, y -> x - y }
            Operation.MUL -> { x, y -> x * y }
            Operation.DIV -> { x, y -> x / y }
            Operation.MOD -> { x, y -> x % y }
            Operation.LT -> { x, y -> (x < y).toInt() }
            Operation.LTE -> { x, y -> (x <= y).toInt() }
            Operation.GT -> { x, y -> (x > y).toInt() }
            Operation.GTE -> { x, y -> (x >= y).toInt() }
            Operation.EQ -> { x, y -> (x == y).toInt() }
            Operation.NEQ -> { x, y -> (x != y).toInt() }
            Operation.AND -> { x, y -> (x.toBool() && y.toBool()).toInt() }
            Operation.OR -> { x, y -> (x.toBool() || y.toBool()).toInt() }
        }
    }

    private fun evalBuiltIn(name: Builtin, arrayArgsNum: Int? = null) {
        when (name) {
            Builtin.READ -> {
                val x = state.inputStream.removeFirst()
                state.opStack.addLast(Value.IntVal(x))
            }

            Builtin.WRITE -> {
                val x = state.opStack.removeLast().asIntVal()
                state.outputStream.add(x)
                state.opStack.addLast(Value.Empty)
            }

            Builtin.ELEM -> {
                val index = state.opStack.removeLast().asIntVal()
                val arrayLike = state.opStack.removeLast()
                when (arrayLike) {
                    is Value.StringVal -> state.opStack.addLast(Value.IntVal(arrayLike.value[index].toInt())) // TODO Maybe I shouldn't fill with sign bit
                    is Value.Array -> state.opStack.addLast(arrayLike.value[index])
                    is Value.Sexp -> state.opStack.addLast(arrayLike.value[index])
                    else -> throw IllegalArgumentException("Value $arrayLike is not array-like")
                }
            }

            Builtin.LENGTH -> {
                val arrayLike = state.opStack.removeLast()
                val r = when (arrayLike) {
                    is Value.StringVal -> Value.IntVal(arrayLike.value.size)
                    is Value.Array -> Value.IntVal(arrayLike.value.size)
                    is Value.Sexp -> Value.IntVal(arrayLike.value.size)
                    else -> throw IllegalArgumentException("Value $arrayLike is not array-like")
                }
                state.opStack.addLast(r)
            }

            Builtin.ARRAY -> state.opStack.removeLast(arrayArgsNum!!).reversed()
                .let { state.opStack.addLast(Value.Array(it.toTypedArray())) }

            Builtin.STRING -> state.opStack.addLast(
                Value.StringVal(
                    state.opStack.removeLast().toString().toByteArray()
                )
            )
        }
    }

    private fun updateElem(arrayLike: Value, index: Int, value: Value) {
        when (arrayLike) {
            is Value.Array -> arrayLike.value[index] = value
            is Value.Sexp -> arrayLike.value[index] = value
            is Value.StringVal -> arrayLike.value[index] = (value as Value.IntVal).value.toByte()
            else -> throw IllegalArgumentException("Cannot update $arrayLike at $index with $value")
        }
    }
}

private fun <T> ArrayDeque<T>.removeLast(n: Int) = (1..n).map { removeLast() }

class InterpreterException(message: String) : RuntimeException(message)

class LamaException(message: String) : RuntimeException(message)

sealed interface Value {
    data object Empty : Value
    data class Var(val designation: Designation) : Value
    data class Elem(val v: Value, val index: Int) : Value
    data class IntVal(val value: Int) : Value
    data class StringVal(val value: ByteArray) : Value
    data class Array(val value: kotlin.Array<Value>) : Value
    class Sexp(val tag: String, val value: kotlin.Array<Value>) : Value {
        override fun toString(): String {
            return "Sexp${super.toString().drop(55)}(tag='$tag', value=$value)"
        }
    }

    data class Closure(val offset: Int, val closure: List<Value>) : Value
    data class FunRef(val name: String, val args: List<String>, val body: String, val index: Int) : Value
    data class Builtin(val name: cub.virtual.machines.Builtin) : Value

    fun asIntVal() =
        if (this is IntVal) value else throw IllegalArgumentException("Cannot convert $this to integer")
}

data class Local(val args: Array<Value>, var locals: Array<Value>, val closure: Array<Value>)

data class StackFrame(val callOffset: Int, val local: Local)

data class State(
    val controlStack: ArrayDeque<StackFrame>,
    val opStack: ArrayDeque<Value>,
    val globals: MutableMap<Int, Value>,
    var local: Local,
    val inputStream: MutableList<Int>,
    val outputStream: MutableList<Int>,
) {
    fun bind(x: Int, v: Value) { // TODO It may be incorrect
        globals.put(x, v)
    }

    fun update(x: Designation, v: Value) {
        when (x) {
            is Designation.Global -> bind(x.index, v)
            is Designation.Local -> local.locals[x.index] = v
            is Designation.Access -> local.closure[x.index] = v
            is Designation.Arg -> local.args[x.index] = v
            is Designation.Fun -> TODO()
        }
    }
}
