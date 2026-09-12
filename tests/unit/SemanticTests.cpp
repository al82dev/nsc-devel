#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/Sema/ConversionKind.h"

#include "../support/TestContext.h"

#include <initializer_list>
#include <string>

namespace
{
bool accepts(std::string source)
{
    nativesharp::frontend::FrontendCompilation compilation;
    return compilation.compileBuffer("semantic.ns", std::move(source));
}

bool rejectsWith(std::string source, nativesharp::diagnostics::DiagnosticId expected)
{
    nativesharp::frontend::FrontendCompilation compilation;
    const bool ok = compilation.compileBuffer("semantic.ns", std::move(source));
    if (ok)
    {
        return false;
    }

    for (const nativesharp::diagnostics::Diagnostic& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.id() == expected)
        {
            return true;
        }
    }
    return false;
}
}

int main()
{
    using nativesharp::diagnostics::DiagnosticId;
    using nativesharp::tests::TestContext;

    TestContext tests;

    tests.expect(accepts("int Main() { return 42; }"), "P00 return constant is accepted");
    tests.expect(accepts("int Add(int a, int b) { return a + b; }"), "function parameters and addition are accepted");
    tests.expect(accepts("int F(); int G() { return F(); } int F() { return 1; }"),
        "prototype enables declaration-before-use");
    tests.expect(rejectsWith("int G() { return F(); } int F() { return 1; }", DiagnosticId::UnknownIdentifier),
        "calling later function without prototype is rejected");
    tests.expect(accepts("int F(int n) { if (n == 0) { return 0; } return F(n - 1); }"),
        "recursion is accepted");

    tests.expect(rejectsWith(
        "int Main() { int x = 1; if (x) { return 0; } return 1; }",
        DiagnosticId::InvalidCondition),
        "conditions require bool");
    tests.expect(rejectsWith("int Main() { break; return 0; }", DiagnosticId::BreakOutsideLoop),
        "break outside a loop is rejected");
    tests.expect(rejectsWith("int Main() { continue; return 0; }", DiagnosticId::ContinueOutsideLoop),
        "continue outside a loop is rejected");

    tests.expect(accepts("int F() { while (true) { } }"), "provably infinite while prevents non-void fallthrough");
    tests.expect(accepts("int F() { for (;;) { } }"), "provably infinite for prevents non-void fallthrough");
    tests.expect(rejectsWith("int F(bool b) { if (b) { return 1; } }", DiagnosticId::NotAllPathsReturn),
        "reachable non-void fallthrough is rejected");
    tests.expect(rejectsWith("int F() { while (true) { break; } }", DiagnosticId::NotAllPathsReturn),
        "break makes infinite loop fall through");
    tests.expect(accepts("int F() { while (true) { if (false) { break; } } }"),
        "unreachable constant-false break does not create fallthrough");

    tests.expect(accepts("const int A = 1; const int B = A + 2; int Main() { return B; }"),
        "const symbols participate in constant expressions");
    tests.expect(rejectsWith("int x = Get(); int Get() { return 1; }", DiagnosticId::UnknownIdentifier),
        "global initializer cannot call undeclared function");
    tests.expect(rejectsWith("int F() { return 1; } int x = F();", DiagnosticId::GlobalRequiresConstant),
        "global initializer must be constant");
    tests.expect(rejectsWith(
        "int Main() { int x = 1; const int y = x; return y; }",
        DiagnosticId::ConstRequiresConstant),
        "const local initializer must be constant");

    tests.expect(accepts("int Main() { nuint x = 0; if (x == 0u) { return 1; } return 0; }"),
        "uint and nuint use nuint as their common integral type");
    tests.expect(rejectsWith(
        "int Main() { nuint x = 0; bool equal = x == 0; return 0; }",
        DiagnosticId::InvalidOperand),
        "integer constants do not override the normative common-integral-type algorithm");
    tests.expect(rejectsWith(
        "int Main() { uint u = 1u; uint r = 1 + u; return (int)r; }",
        DiagnosticId::InvalidConversion),
        "int plus uint has type long even when the int operand is a constant");
    tests.expect(accepts("int Main() { byte b = 255; b += 1; return b; }"),
        "compound assignment converts result back to lhs type");
    tests.expect(rejectsWith("int Main() { bool b = (bool)1; return 0; }", DiagnosticId::InvalidConversion),
        "numeric to bool cast is rejected");
    tests.expect(rejectsWith(
        "int Main() { int x = (int)(1.0 / 0.0); return x; }",
        DiagnosticId::ConstantEvaluationTrap),
        "known floating-to-integer trap is diagnosed");
    tests.expect(accepts("int Main() { nuint x = sizeof((int)(1.0 / 0.0)); return (int)x; }"),
        "sizeof suppresses traps in unevaluated operand");
    tests.expect(accepts("int Main() { nuint x = alignof((int)(1.0 / 0.0)); return (int)x; }"),
        "alignof suppresses traps in unevaluated operand");
    tests.expect(accepts("int Main() { bool b = false && (1 / 0 == 0); if (b) { return 1; } return 0; }"),
        "constant short-circuit suppresses unevaluated trap");
    tests.expect(rejectsWith("const int X = 1 / 0; int Main() { return X; }", DiagnosticId::ConstantEvaluationTrap),
        "constant integer division by zero is diagnosed");
    tests.expect(rejectsWith("const int X = 1 << 32; int Main() { return X; }", DiagnosticId::ConstantEvaluationTrap),
        "constant invalid shift count is diagnosed");

    tests.expect(accepts("struct P { int X; int Y; } int Main() { P a; P b; a = b; return 0; }"),
        "same nominal struct type is assignable by value");
    tests.expect(rejectsWith(
        "struct A { int X; } struct B { int X; } int Main() { A a; B b; a = b; return 0; }",
        DiagnosticId::InvalidConversion),
        "different nominal structs are not assignable");
    tests.expect(rejectsWith("struct Node { Node Next; } int Main() { return 0; }", DiagnosticId::IncompleteType),
        "direct recursive struct field is rejected");
    tests.expect(accepts("struct Node { Node* Next; } int Main() { Node n; n.Next = null; return 0; }"),
        "self pointer in struct is accepted");

    tests.expect(accepts("int Main() { int[4] a; a[3] = 1; return a[3]; }"),
        "fixed-array indexing is accepted in bounds");
    tests.expect(rejectsWith("int Main() { int[4] a; return a[4]; }", DiagnosticId::ConstantIndexOutOfBounds),
        "constant fixed-array out-of-bounds index is rejected");
    tests.expect(rejectsWith("int Main() { int[4] a; int* p = a; return 0; }", DiagnosticId::InvalidConversion),
        "fixed array does not decay to pointer");
    tests.expect(rejectsWith(
        "int Main() { int[4] a; int[4] b; a = b; return 0; }",
        DiagnosticId::InvalidAssignmentTarget),
        "whole fixed array assignment is rejected");
    tests.expect(accepts("int Main() { char c = \"abc\"[1]; return (int)c; }"),
        "string literal supports array-like indexing");
    tests.expect(rejectsWith("int Main() { \"abc\"[0] = 'x'; return 0; }", DiagnosticId::InvalidAssignmentTarget),
        "string literal storage is non-modifiable");

    tests.expect(accepts("int Main() { int x = 1; int* p = &x; *p = 2; return x; }"),
        "address-of and dereference are accepted");
    tests.expect(rejectsWith("int Main() { void* p = null; p = p + 1; return 0; }", DiagnosticId::InvalidOperand),
        "pointer arithmetic on void pointer is rejected");
    tests.expect(accepts("int Main() { int* p = null; void* q = p; p = (int*)q; return 0; }"),
        "T* to void* implicit and void* to T* explicit conversions are accepted");
    tests.expect(rejectsWith("int Main() { int* p = null; float* q = null; bool b = p == q; return 0; }",
        DiagnosticId::InvalidOperand),
        "different non-void pointer types cannot be compared implicitly");

    tests.expect(accepts("enum E { A, B = A + 2, Third, } int Main() { E e = E.Third; return (int)e; }"),
        "enum auto values and previous-member constants are accepted");
    tests.expect(accepts(
        "enum E { A } const int X = (int)E.A; const nuint S = sizeof(E.A); int Main() { return X + (int)S; }"),
        "enum member casts and sizeof(enum-member expression) are constant expressions");
    tests.expect(rejectsWith("enum E { A = 1.5 } int Main() { return 0; }", DiagnosticId::InvalidEnumInitializer),
        "enum initializer must be an integer constant expression");
    tests.expect(rejectsWith(
        "enum E { Max = 2147483647, Overflow, } int Main() { return 0; }",
        DiagnosticId::InvalidEnumInitializer),
        "auto-incremented enum values must remain representable as int");

    tests.expect(accepts(
        "int Main() { int* p = new int[0]; int* q = stackalloc int[0]; delete[] p; delete q; return 0; }"),
        "zero-count heap and stack allocation are accepted by 04 semantics");
    tests.expect(rejectsWith("int Main() { int* p = new int[-1]; return 0; }", DiagnosticId::ConstantEvaluationTrap),
        "negative constant new[] count is rejected");
    tests.expect(rejectsWith(
        "int Main() { int* p = stackalloc int[-1]; return 0; }",
        DiagnosticId::ConstantEvaluationTrap),
        "negative constant stackalloc count is rejected");
    tests.expect(rejectsWith("int Main() { long* p = new long[9223372036854775807L]; return 0; }",
        DiagnosticId::ConstantEvaluationTrap),
        "known allocation byte-size overflow is rejected");
    tests.expect(accepts("int Main() { delete null; delete[] null; return 0; }"),
        "delete null and delete[] null are statically valid");

    tests.expect(accepts("extern C int puts(char* s); int Main() { puts(\"ok\"); return 0; }"),
        "string literal converts specially to char* for C interop");
    tests.expect(rejectsWith("extern C int f(int x); int Main() { return f(); }", DiagnosticId::InvalidArgumentCount),
        "function argument count is checked");
    tests.expect(rejectsWith("int Main(int argc) { return argc; }", DiagnosticId::InvalidMainSignature),
        "Main signature is checked");

    tests.expect(rejectsWith("void x; int Main() { return 0; }", DiagnosticId::InvalidType),
        "void object declarations are rejected");
    tests.expect(accepts("void* p; int Main() { p = null; return 0; }"),
        "void pointers remain valid object-pointer values");
    tests.expect(rejectsWith("int[2][3] matrix; int Main() { return 0; }", DiagnosticId::InvalidArrayCount),
        "nested fixed arrays are rejected in NativeSharp 1.0.1");
    tests.expect(rejectsWith("int[0] empty; int Main() { return 0; }", DiagnosticId::InvalidArrayCount),
        "fixed arrays require a positive constant count");
    tests.expect(rejectsWith("int[n] values; int n; int Main() { return 0; }", DiagnosticId::UnknownIdentifier),
        "array type count obeys declaration-before-use and constant-expression rules");

    tests.expect(rejectsWith("int[2] F(); int Main() { return 0; }", DiagnosticId::InvalidType),
        "functions cannot return fixed arrays by value");
    tests.expect(rejectsWith("void F(int[2] a) { } int Main() { return 0; }", DiagnosticId::InvalidType),
        "functions cannot receive fixed arrays by value");
    tests.expect(accepts(
        "struct P { int X; } P Echo(P p) { return p; } int Main() { P p; p.X = 1; return Echo(p).X; }"),
        "complete structs can be passed and returned by value");
    tests.expect(rejectsWith(
        "int F(int x); int F(float x); int Main() { return 0; }",
        DiagnosticId::ConflictingFunctionDeclaration),
        "function overloading is rejected");
    tests.expect(rejectsWith("int F() { return 1; } int F() { return 2; } int Main() { return 0; }",
        DiagnosticId::DuplicateFunctionDefinition),
        "multiple function definitions are rejected");
    tests.expect(rejectsWith("extern C int F(int x); int F(int x); int Main() { return 0; }",
        DiagnosticId::ConflictingFunctionDeclaration),
        "language-linkage conflicts are rejected");
    tests.expect(rejectsWith("int F() { return 1; } int Main() { int x = F; return x; }", DiagnosticId::InvalidOperand),
        "function designators cannot be ordinary runtime values");

    tests.expect(accepts("int Main() { int[10] a; int[10]* p = &a; return 0; }"),
        "address of a whole fixed array has pointer-to-array type");
    tests.expect(accepts(
        "int Main() { int[10] a; int* first = &a[0]; int* last = first + 10; "
        "nint n = last - first; return (int)n; }"),
        "pointer arithmetic and pointer subtraction are type-correct for the same pointer type");
    tests.expect(rejectsWith("int Main() { int* a = null; int* b = null; bool less = a < b; return 0; }",
        DiagnosticId::InvalidOperand),
        "pointer relational ordering is not defined");
    tests.expect(accepts("int Main() { int* p = null; nuint bits = (nuint)p; p = (int*)bits; return 0; }"),
        "pointer-native-integer casts are accepted");
    tests.expect(rejectsWith(
        "int Main() { int* p = null; long bits = (long)p; return 0; }",
        DiagnosticId::InvalidConversion),
        "pointer-to-long cast is rejected; native integers are required");

    tests.expect(accepts("enum E { A, B } int Main() { bool b = E.A == E.B; if (b) { return 1; } return 0; }"),
        "equality is valid within one enum type");
    tests.expect(rejectsWith("enum A { X } enum B { X } int Main() { bool b = A.X == B.X; return 0; }",
        DiagnosticId::InvalidOperand),
        "different enum types cannot be compared directly");
    tests.expect(accepts("enum E { A } int Main() { int x = (int)E.A + 1; E e = E.A; e = (E)x; return (int)e; }"),
        "explicit enum/integer conversion is accepted in both directions");
    tests.expect(rejectsWith("enum E { A } int Main() { E e = E.A; e + 1; return 0; }", DiagnosticId::InvalidOperand),
        "enum arithmetic is rejected without an explicit cast");
    tests.expect(rejectsWith("enum E { A, A } int Main() { return 0; }", DiagnosticId::DuplicateSymbol),
        "duplicate enum member names are rejected");
    tests.expect(rejectsWith("struct S { int X; int X; } int Main() { return 0; }", DiagnosticId::DuplicateSymbol),
        "duplicate struct field names are rejected");

    tests.expect(accepts("int Main() { byte x = 42; return x; }"),
        "implicit constant integer narrowing succeeds when value is representable");
    tests.expect(rejectsWith("int Main() { byte x = 300; return x; }", DiagnosticId::InvalidConversion),
        "implicit constant integer narrowing rejects out-of-range values");
    tests.expect(rejectsWith("int Main() { byte b = 65; char c = b; return (int)c; }", DiagnosticId::InvalidConversion),
        "runtime byte-to-char conversion is not implicit");
    tests.expect(rejectsWith("int Main() { long a = 1; ulong b = 2u; a + b; return 0; }", DiagnosticId::InvalidOperand),
        "long and ulong do not have an implicit common integral type");
    tests.expect(rejectsWith(
        "int Main() { float a = 1.0f; float b = 2.0f; a % b; return 0; }",
        DiagnosticId::InvalidOperand),
        "floating remainder is rejected");
    tests.expect(accepts("const int X = 2147483647 + 1; int Main() { return X; }"),
        "constant signed addition wraps modulo 2^N");
    tests.expect(rejectsWith(
        "const long X = (-9223372036854775807L - 1L) / -1L; int Main() { return 0; }",
        DiagnosticId::ConstantEvaluationTrap),
        "signed MIN divided by -1 is diagnosed during constant evaluation");

    tests.expect(rejectsWith("int Main() { nuint s = sizeof(void); return (int)s; }", DiagnosticId::InvalidSizeof),
        "sizeof(void) is rejected");
    tests.expect(rejectsWith("int F() { return 0; } int Main() { nuint s = sizeof(F); return (int)s; }",
        DiagnosticId::InvalidSizeof),
        "sizeof(function designator) is rejected");
    tests.expect(accepts("struct P { byte A; int B; } int Main() { const nuint s = sizeof(P); return (int)s; }"),
        "sizeof(struct) is a compile-time value derived from target layout");
    tests.expect(accepts(
        "struct P { byte A; int B; } int Main() { const nuint a = alignof(P); return (int)a; }"),
        "alignof(struct) is a compile-time value derived from the same target layout");
    tests.expect(rejectsWith("int Main() { nuint a = alignof(void); return (int)a; }", DiagnosticId::InvalidSizeof),
        "alignof(void) is rejected");

    tests.expect(accepts("int x = 1; int Main() { int x = 2; { int x = 3; } return x; }"),
        "ordinary value names may shadow in strictly nested scopes");
    tests.expect(rejectsWith("struct P { int X; } int Main() { int P = 1; return P; }", DiagnosticId::DuplicateSymbol),
        "visible type names cannot be shadowed by values");
    tests.expect(rejectsWith("int F(int x) { int x = 1; return x; }", DiagnosticId::DuplicateSymbol),
        "parameters cannot be redeclared in the function outer block");

    {
        nativesharp::frontend::FrontendCompilation compilation;
        const bool ok = compilation.compileBuffer(
            "conversion_metadata.ns",
            "int Main() { long x = 42; byte b = 1; int y = b + 2; return (int)(x + y); }");
        tests.expect(ok, "conversion-metadata program is semantically valid");
        if (ok)
        {
            const nativesharp::ast::AstNode* function = compilation.syntaxTree()->children().front();
            const nativesharp::ast::AstNode* body = function->first();
            const nativesharp::ast::AstNode* longDeclaration = body->children()[0];
            const auto conversions = compilation.semanticAnalyzer()->model().conversions(longDeclaration);
            tests.expect(conversions.size() == 1, "semantic model records contextual initializer conversion");
            if (!conversions.empty())
            {
                tests.expect(
                    conversions.front().kind == nativesharp::sema::ConversionKind::ImplicitNumeric,
                    "int-to-long initializer is classified as implicit numeric conversion");
                tests.expect(
                    conversions.front().sourceType->displayName() == "int" &&
                        conversions.front().destinationType->displayName() == "long",
                    "conversion metadata preserves source and destination types");
            }

            const nativesharp::ast::AstNode* intDeclaration = body->children()[2];
            const nativesharp::ast::AstNode* addExpression = intDeclaration->first();
            const auto addConversions = compilation.semanticAnalyzer()->model().conversions(addExpression);
            tests.expect(!addConversions.empty(), "binary integral promotion is recorded in semantic model");
            const auto* resolved = compilation.semanticAnalyzer()->model().getOperator(addExpression);
            tests.expect(resolved != nullptr, "semantic model records selected binary operator information");
            if (resolved != nullptr)
            {
                tests.expect(
                    resolved->operationType->displayName() == "int" && resolved->resultType->displayName() == "int",
                    "resolved binary operator preserves operation and result types");
            }
        }
    }

    tests.expect(accepts(
        "struct P { int X; int Y; } extern C P LoadPoint(); extern C void SavePoint(P p); "
        "int Main() { return 0; }"),
        "non-empty ABI-representable structs may cross extern C by value");
    tests.expect(rejectsWith(
        "struct Empty { } extern C Empty GetEmpty(); int Main() { return 0; }",
        DiagnosticId::InvalidExternCSignature),
        "empty struct return by value is rejected at extern C boundary");
    tests.expect(rejectsWith(
        "struct Empty { } extern C void PutEmpty(Empty value); int Main() { return 0; }",
        DiagnosticId::InvalidExternCSignature),
        "empty struct parameter by value is rejected at extern C boundary");
    tests.expect(accepts(
        "struct Empty { } extern C void UsePointer(Empty* value); int Main() { return 0; }"),
        "pointer to empty struct remains ABI-expressible across extern C");
    tests.expect(rejectsWith(
        "struct Empty { } struct Outer { Empty E; } extern C void PutOuter(Outer value); "
        "int Main() { return 0; }",
        DiagnosticId::InvalidExternCSignature),
        "extern C struct compatibility is checked recursively through fields");
    tests.expect(accepts(
        "struct Packet { int[4] Values; } extern C void PutPacket(Packet value); int Main() { return 0; }"),
        "fixed-array fields inside an otherwise ABI-compatible struct are accepted");
    tests.expect(accepts("extern C void SetReady(bool value); int Main() { return 0; }"),
        "one-byte NativeSharp bool is an ABI-expressible extern C scalar");

    {
        nativesharp::frontend::FrontendCompilation compilation;
        const bool ok = compilation.compileBuffer(
            "dynamic_bounds.ns",
            "int Main() { int[4] values; int i = 1; values[i] = 7; return values[i]; }");
        tests.expect(ok, "dynamic fixed-array indexing is semantically valid");
        if (ok)
        {
            const nativesharp::ast::AstNode* function = compilation.syntaxTree()->children().front();
            const nativesharp::ast::AstNode* body = function->first();
            const nativesharp::ast::AstNode* assignment = body->children()[2]->first();
            const nativesharp::ast::AstNode* indexed = assignment->first();
            const auto* info = compilation.semanticAnalyzer()->model().get(indexed);
            tests.expect(info != nullptr && info->requiresBoundsCheck,
                "semantic model marks runtime fixed-array indexing for mandatory bounds checking");
        }
    }

    {
        nativesharp::frontend::FrontendCompilation compilation;
        const bool ok = compilation.compileBuffer(
            "layout_metadata.ns",
            "struct Mixed { byte A; int B; short Tail; } int Main() { Mixed value; return 0; }");
        tests.expect(ok, "struct layout metadata program is semantically valid");
        if (ok)
        {
            const auto globals = compilation.semanticAnalyzer()->globalSymbols();
            const nativesharp::sema::Symbol* mixed = nullptr;
            for (const nativesharp::sema::Symbol* symbol : globals)
            {
                if (symbol->name() == "Mixed")
                {
                    mixed = symbol;
                    break;
                }
            }
            tests.expect(mixed != nullptr && mixed->members().size() == 3,
                "struct symbol retains all field symbols");
            if (mixed != nullptr && mixed->members().size() == 3)
            {
                tests.expect(mixed->members()[0]->hasFieldOffset() && mixed->members()[0]->fieldOffset() == 0,
                    "first struct field offset comes from TargetDataLayout");
                tests.expect(mixed->members()[1]->hasFieldOffset() && mixed->members()[1]->fieldOffset() == 4,
                    "aligned struct field offset includes internal padding");
                tests.expect(mixed->members()[2]->hasFieldOffset() && mixed->members()[2]->fieldOffset() == 8,
                    "third struct field offset is preserved for later lowering");
            }
        }
    }

    {
        nativesharp::frontend::FrontendCompilation compilation;
        const bool ok = compilation.compileBuffer(
            "previous_declaration.ns",
            "int F(int x); float F(float x); int Main() { return 0; }");
        tests.expect(!ok, "conflicting function declaration is rejected");
        bool hasConflict = false;
        bool hasPreviousNote = false;
        for (const nativesharp::diagnostics::Diagnostic& diagnostic : compilation.diagnostics().diagnostics())
        {
            hasConflict = hasConflict || diagnostic.id() == DiagnosticId::ConflictingFunctionDeclaration;
            hasPreviousNote = hasPreviousNote || diagnostic.id() == DiagnosticId::PreviousDeclaration;
        }
        tests.expect(hasConflict, "conflicting declaration emits the primary error");
        tests.expect(hasPreviousNote, "conflicting declaration emits a previous-declaration note");
    }

    {
        nativesharp::frontend::FrontendCompilation compilation;
        const bool ok = compilation.compileBuffer(
            "void_cast_metadata.ns",
            "int Main() { int x = 1; (void)x; return 0; }");
        tests.expect(ok, "explicit cast to void is accepted");
        if (ok)
        {
            const nativesharp::ast::AstNode* function = compilation.syntaxTree()->children().front();
            const nativesharp::ast::AstNode* castExpression = function->first()->children()[1]->first();
            const auto conversions = compilation.semanticAnalyzer()->model().conversions(castExpression);
            tests.expect(conversions.size() == 1, "cast-to-void conversion is represented explicitly");
            if (!conversions.empty())
            {
                tests.expect(conversions.front().kind == nativesharp::sema::ConversionKind::ToVoid,
                    "cast-to-void metadata uses ToVoid conversion kind");
            }
        }
    }

    return tests.result();
}
