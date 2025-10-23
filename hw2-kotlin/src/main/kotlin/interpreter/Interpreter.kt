package cub.virtual.machines.interpreter

import cub.virtual.machines.Designation
import cub.virtual.machines.Instruction
import cub.virtual.machines.Instruction.*
import cub.virtual.machines.Operation

class Interpreter(input: List<Int>) {
    private val state: State = State(
        controlStack = ArrayDeque(),
        opStack = ArrayDeque(),
        globals = mutableMapOf(), // fill with builtins
        local = Local(mutableListOf(), mutableListOf(), mutableListOf()),
        inputStream = input.toMutableList(),
        outputStream = mutableListOf()
    )

    fun interpret(operation: Operation): (Int, Int) -> Int {
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
        }
    }

    enum class BuiltIn(val repr: String) {
        READ("read"), WRITE("write"), ELEM(".elem"), LENGTH("length"), ARRAY(".array"), STRING("string")
    }

    fun evalBuiltIn(name: String) {
        when (name) {
            "read" -> {
                val x = state.inputStream.removeFirst()
                Value.IntVal(x)
            }

            "write" -> {
                val x = state.opStack.removeLast().asIntVal()
                state.outputStream.add(x)
                Value.Empty
            }

            ".elem" -> {
                val arrayLike = state.opStack.removeLast()
                val index = state.opStack.removeLast().asIntVal()
                when (arrayLike) {
                    is Value.StringVal -> state.opStack.addLast(Value.IntVal(arrayLike.value[index].toInt())) // TODO Maybe I shouldn't fill with sign bit
                    is Value.Array -> state.opStack.addLast(arrayLike.value[index])
                    is Value.Sexp -> state.opStack.addLast(arrayLike.value[index])
                    else -> throw IllegalArgumentException("Value $arrayLike is not array-like")
                }
            }

            "length" -> {
                val arrayLike = state.opStack.removeLast()
                when (arrayLike) {
                    is Value.StringVal -> Value.IntVal(arrayLike.value.size)
                    is Value.Array -> Value.IntVal(arrayLike.value.size)
                    is Value.Sexp -> Value.IntVal(arrayLike.value.size)
                    else -> throw IllegalArgumentException("Value $arrayLike is not array-like")
                }
            }
            ".array" -> {TODO()}
            "string" -> {TODO()}
            else -> throw IllegalArgumentException("Unknown built-in $name")
        }
    }

    class InterpreterException(instruction: Instruction, message: String) : RuntimeException("Error interpreting $instruction: $message") // TODO print IP

//    let update_elem x i v =
//    match x with
//    | Sexp (_, a) | Array a -> ignore (update_array a i v)
//    | String a -> ignore (update_string a i (Char.chr @@ to_int v))
//    | _ -> failwith (Printf.sprintf "Unexpected pattern: %s: %d" __FILE__ __LINE__)

    fun updateElem(arrayLike: Value, index: Int, value: Value) {
        when (arrayLike) {
            is Value.Array -> arrayLike.value[index] = value
            is Value.Sexp -> arrayLike.value[index] = value
            is Value.StringVal -> arrayLike.value[index] = (value as Value.IntVal).value.toByte()
            else -> throw IllegalArgumentException("Cannot update $arrayLike at $index with $value")
        }
    }

    fun interpret(instruction: Instruction) {

        when (instruction) {
            is IMPORT, is PUBLIC, is EXTERN, is LINE -> Unit
            is BINOP -> {
                val y = state.opStack.removeLast()
                val x = state.opStack.removeLast()
                when (instruction.op) {
                    Operation.EQ -> {
                        val z = if (x is Value.IntVal && y is Value.IntVal) {
                            interpret(instruction.op)(x.value, y.value)
                        } else if (x is Value.IntVal || y is Value.IntVal) {
                            0
                        } else {
                            throw IllegalArgumentException("Cannot compare non-integer values $x and $y")
                        }

                        state.opStack.addLast(Value.IntVal(z))
                    }

                    else -> {
                        interpret(instruction.op)(x.asIntVal(), y.asIntVal())
                    }
                }
            }

            is CONST -> state.opStack.addLast(Value.IntVal(instruction.value))
            is STRING -> state.opStack.addLast(Value.StringVal(instruction.value.toByteArray()))
            is SEXP -> state.opStack.addLast(
                Value.Sexp(
                    instruction.tag,
                    state.opStack.takeLast(instruction.n).reversed().toMutableList()
                )
            )

            ELEM -> evalBuiltIn(".elem")
            is LD -> {
                when (instruction.designation) {
                    is Designation.Global -> state.globals[instruction.designation.index] ?: throw IllegalArgumentException("Unknown global ${instruction.designation.index}")
                    is Designation.Local -> state.local.locals[instruction.designation.index]
                    is Designation.Access -> state.local.closure[instruction.designation.index]
                    is Designation.Arg -> state.local.args[instruction.designation.index]
                    is Designation.Fun -> throw IllegalArgumentException("Cannot load ${instruction.designation} on stack")
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
                val value  = state.opStack.removeLast()
                val variable  = state.opStack.removeLast()
                when (variable) {
                    is Value.Var -> state.update(variable.designation, value).also { state.opStack.addLast(value) }
                    is Value.IntVal -> {
                        val array = state.opStack.removeLast()
                        updateElem(array, variable.value, value)
                        state.opStack.addLast(value)
                    }
                    else -> throw InterpreterException(instruction, "Unknown arg for STA $variable")
                }
            }
            is SLABEL, is FLABEL, is LABEL -> Unit
            is JMP -> TODO()
            is ARRAY -> TODO()
            is BEGIN -> TODO()
            is CALL -> TODO()
            is CALLC -> TODO()
            is CJMP -> TODO()
            is CLOSURE -> TODO()
            DROP -> TODO()
            DUP -> TODO()
            END -> TODO()
            is FAIL -> TODO()
            is PATT -> TODO()
            RET -> TODO()
            SWAP -> TODO()
            is TAG -> TODO()
        }

    }

    sealed interface Value {
        //        | Empty
//        | Var     of designation
//        | Elem    of ('a, 'b) t * int
//        | Int     of int
//        | String  of bytes
//        | Array   of ('a, 'b) t array
//        | Sexp    of string * ('a, 'b) t array
//        | Closure of string list * 'a * 'b
//        | FunRef  of string * string list * 'a * int
//        | Builtin of string
        data object Empty : Value
        data class Var(val designation: Designation) : Value
        data class Elem(val v: Value, val index: Int) : Value
        data class IntVal(val value: Int) : Value
        data class StringVal(val value: ByteArray) : Value
        data class Array(val value: MutableList<Value>) : Value
        data class Sexp(val tag: String, val value: MutableList<Value>) : Value
        data class Closure(val args: List<String>, val body: String, val c: List<Value>) : Value
        data class FunRef(val name: String, val args: List<String>, val body: String, val index: Int) : Value
        data class Builtin(val name: String) : Value

        fun asIntVal() =
            if (this is IntVal) value else throw IllegalArgumentException("Cannot convert $this to integer")
    }

    data class Local(val args: MutableList<Value>, val locals: MutableList<Value>, val closure: MutableList<Value>)

    data class Prg(val instructionIndex: Int)

    data class StackFrame(val program: Prg, val local: Local)

    data class State(
        val controlStack: ArrayDeque<StackFrame>,
        val opStack: ArrayDeque<Value>,
        val globals: MutableMap<Int, Value>,
        val local: Local,
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
}

