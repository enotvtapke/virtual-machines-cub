package cub.virtual.machines.decompiler

sealed interface Instruction {
    data class BINOP(val op: Operation) : Instruction
    data class CONST(val value: Int) : Instruction
    data class STRING(val value: String) : Instruction
    data class SEXP(val op: String, val arity: Int) : Instruction
    //(* load a variable to the stack              *)
    //| LD of Value.designation
    data class LD(val designation: Designation) : Instruction
    //(* load a variable address to the stack      *)
    //| LDA of Value.designation
    data class LDA(val designation: Designation) : Instruction
    //(* store a value into a variable             *)
    //| ST of Value.designation
    data class ST(val designation: Designation) : Instruction
    //(* store a value into a reference            *)
    //| STI
    data object STI : Instruction
    //(* store a value into array/sexp/string      *)
    //| STA
    data object STA : Instruction
    //(* takes an element of array/string/sexp     *)
    //| ELEM
    data object ELEM : Instruction
    //(* a label                                   *)
    //| LABEL of string
    data class LABEL(val name: String) : Instruction
    //(* a forwarded label                         *)
    //| FLABEL of string
    data class FLABEL(val name: String) : Instruction
    //(* a scope label                             *)
    //| SLABEL of string
    data class SLABEL(val name: String) : Instruction
    //(* unconditional jump                        *)
    //| JMP of string
    data class JMP(val position: Int) : Instruction
    //(* conditional jump                          *)
    //| CJMP of string * string
    data class CJMP(val cond: String, val position: Int) : Instruction
    //(* begins procedure definition               *)
    //| BEGIN of
    //string * int * int * Value.designation list * string list * scope list
    data class BEGIN(val arity: Int, val locals: Int) : Instruction
    //(* end procedure definition                  *)
    //| END
    data object END : Instruction
    //(* create a closure                          *)
    //| CLOSURE of string * Value.designation list
    data class CLOSURE(val l: Int, val n: Int, val closureArgs: List<Designation>) : Instruction
    //(* proto closure                             *)
    //| PROTO of string * string
    //(* proto closure to a possible constant      *)
    //| PPROTO of string * string
    //(* proto call                                *)
    //| PCALLC of int * bool
    //(* calls a closure                           *)
    //| CALLC of int * bool
    data class CALLC(val n: Int) : Instruction
    //(* calls a function/procedure                *)
    //| CALL of string * int * bool
    data class CALL(val l: Int, val n: Int) : Instruction
    //(* returns from a function                   *)
    //| RET
    data object RET : Instruction
    //(* drops the top element off                 *)
    //| DROP
    data object DROP : Instruction
    //(* duplicates the top element                *)
    //| DUP
    data object DUP : Instruction
    //(* swaps two top elements                    *)
    //| SWAP
    data object SWAP : Instruction
    //(* checks the tag and arity of S-expression  *)
    //| TAG of string * int
    data class TAG(val tag: String, val arity: Int) : Instruction
    //(* checks the tag and size of array          *)
    //| ARRAY of int
    data class ARRAY(val size: Int) : Instruction
    //(* checks various patterns                   *)
    //| PATT of patt
    data class PATT(val pattern: Pattern) : Instruction
    //(* match failure (location, leave a value    *)
    //| FAIL of Loc.t * bool
    data class FAIL(val line: Int, val column: Int) : Instruction
    //(* external definition                       *)
    //| EXTERN of string
    data class EXTERN(val name: String) : Instruction
    //(* public   definition                       *)
    //| PUBLIC of string
    data class PUBLIC(val name: String) : Instruction
    //(* import clause                             *)
    //| IMPORT of string
    data class IMPORT(val name: String) : Instruction
    //(* line info                                 *)
    //| LINE of int
    data class LINE(val line: Int) : Instruction
}

enum class Operation {
    ADD, SUB, MUL, DIV, MOD, LT, LTE, GT, GTE, EQ, NEQ, AND, NOT
}

sealed interface Designation {
    data class Global(val index: Int) : Designation
    data class Local(val index: Int) : Designation
    data class Arg(val index: Int) : Designation
    data class Access(val index: Int) : Designation
    data class Fun(val name: String) : Designation
}

enum class Pattern {
    StrCmp,
    String,
    Array,
    Sexp,
    Boxed,
    UnBoxed,
    Closure,
}
