#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/HIR/HirNode.h"
#include "nativesharp/HIR/HirNodeKind.h"
#include "nativesharp/HIR/HirDumper.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Sema/ConversionKind.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/TypeKind.h"
#include "../support/TestContext.h"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <ostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <vector>

using nativesharp::frontend::FrontendCompilation;
using nativesharp::hir::HirNode;
using nativesharp::hir::HirNodeKind;
using nativesharp::lexer::TokenKind;
using nativesharp::sema::ConversionKind;
using nativesharp::sema::TypeKind;

namespace
{
const HirNode* findFirst(const HirNode* node, HirNodeKind kind)
{
    if (node == nullptr)
    {
        return nullptr;
    }
    if (node->kind() == kind)
    {
        return node;
    }
    if (const HirNode* result = findFirst(node->first(), kind)) return result;
    if (const HirNode* result = findFirst(node->second(), kind)) return result;
    if (const HirNode* result = findFirst(node->third(), kind)) return result;
    for (const HirNode* child : node->children())
    {
        if (const HirNode* result = findFirst(child, kind)) return result;
    }
    return nullptr;
}

const HirNode* findOperation(const HirNode* node, HirNodeKind kind, TokenKind operation)
{
    if (node == nullptr)
    {
        return nullptr;
    }
    if (node->kind() == kind && node->operation() == operation)
    {
        return node;
    }
    if (const HirNode* result = findOperation(node->first(), kind, operation)) return result;
    if (const HirNode* result = findOperation(node->second(), kind, operation)) return result;
    if (const HirNode* result = findOperation(node->third(), kind, operation)) return result;
    for (const HirNode* child : node->children())
    {
        if (const HirNode* result = findOperation(child, kind, operation)) return result;
    }
    return nullptr;
}

const HirNode* findFunction(const HirNode* root, std::string_view name)
{
    if (root == nullptr)
    {
        return nullptr;
    }
    if (root->kind() == HirNodeKind::FunctionDeclaration &&
        root->symbol() != nullptr && root->symbol()->name() == name)
    {
        return root;
    }
    for (const HirNode* child : root->children())
    {
        if (const HirNode* result = findFunction(child, name)) return result;
    }
    return nullptr;
}

bool compile(std::string_view source, FrontendCompilation& compilation)
{
    return compilation.compileBuffer(
        std::filesystem::path("hir_test.ns"),
        std::string(source),
        false);
}

class CountingStreamBuffer final : public std::streambuf
{
public:
    /// Returns the number of newline characters accepted by the stream.
    std::size_t lineCount() const noexcept
    {
        return lineCount_;
    }

protected:
    /// Counts an individual character without retaining output bytes.
    int_type overflow(int_type character) override
    {
        if (!traits_type::eq_int_type(character, traits_type::eof()) &&
            traits_type::to_char_type(character) == '\n')
        {
            ++lineCount_;
        }
        return character;
    }

    /// Counts newline characters in a block without retaining output bytes.
    std::streamsize xsputn(const char* text, std::streamsize count) override
    {
        for (std::streamsize index = 0; index < count; ++index)
        {
            if (text[index] == '\n')
            {
                ++lineCount_;
            }
        }
        return count;
    }

private:
    std::size_t lineCount_ = 0;
};

/// Proves that base HIR dumping preserves preorder while using bounded process stack.
void testDeepHirDump(nativesharp::tests::TestContext& tests)
{
    constexpr std::size_t nestingDepth = 8192;
    std::vector<std::unique_ptr<HirNode>> nodes;
    nodes.reserve(nestingDepth + 1U);
    for (std::size_t index = 0; index <= nestingDepth; ++index)
        nodes.push_back(std::make_unique<HirNode>(HirNodeKind::Block, nativesharp::SourceRange{}));
    for (std::size_t index = 0; index < nestingDepth; ++index)
        nodes[index]->setFirst(nodes[index + 1U].get());

    CountingStreamBuffer buffer;
    std::ostream output(&buffer);
    nativesharp::hir::HirDumper{}.dump(nodes.front().get(), output);
    tests.expect(buffer.lineCount() == nestingDepth + 1U,
        "an 8192-level base HIR tree dumps in preorder without process-stack recursion");

    HirNode orderRoot(HirNodeKind::Block, nativesharp::SourceRange{});
    HirNode first(HirNodeKind::Constant, nativesharp::SourceRange{});
    HirNode second(HirNodeKind::Return, nativesharp::SourceRange{});
    HirNode third(HirNodeKind::Unary, nativesharp::SourceRange{});
    HirNode childOne(HirNodeKind::While, nativesharp::SourceRange{});
    HirNode childTwo(HirNodeKind::For, nativesharp::SourceRange{});
    orderRoot.setFirst(&first);
    orderRoot.setSecond(&second);
    orderRoot.setThird(&third);
    orderRoot.addChild(&childOne);
    orderRoot.addChild(&childTwo);
    std::ostringstream orderOutput;
    nativesharp::hir::HirDumper{}.dump(&orderRoot, orderOutput);
    const std::string expected =
        "Block\n"
        "  first: Constant\n"
        "  second: Return\n"
        "  third: Unary\n"
        "  child: While\n"
        "  child: For\n";
    tests.expect(orderOutput.str() == expected,
        "iterative base HIR dumping preserves first/second/third/children order and labels");
}
}

int main()
{
    using nativesharp::tests::TestContext;
    TestContext tests;

    char* focus = nullptr;
    std::size_t focusLength = 0;
    const int focusError = _dupenv_s(&focus, &focusLength, "NSC_HIR_TEST_FOCUS");
    const std::string focusValue = focusError == 0 && focus != nullptr
        ? std::string(focus)
        : std::string{};
    std::free(focus);
    if (focusValue == "hir-base-dump-depth")
    {
        testDeepHirDump(tests);
        return tests.result();
    }

    {
        FrontendCompilation compilation;
        tests.expect(
            compile("int Main() { int x = 10; return x; }", compilation),
            "simple program builds HIR");
        const HirNode* function = findFunction(compilation.hirRoot(), "Main");
        tests.expect(function != nullptr && function->first() != nullptr, "Main HIR function has a body");
        const HirNode* local = findFirst(function, HirNodeKind::LocalVariableDeclaration);
        tests.expect(
            local != nullptr && local->first() != nullptr && local->first()->kind() == HirNodeKind::Constant,
            "local initializer is represented explicitly");
        const HirNode* ret = findFirst(function, HirNodeKind::Return);
        tests.expect(
            ret != nullptr && ret->first() != nullptr && ret->first()->kind() == HirNodeKind::Load,
            "reading a local in return becomes an explicit Load");
        tests.expect(
            ret != nullptr && ret->first() != nullptr && ret->first()->first() != nullptr &&
                ret->first()->first()->kind() == HirNodeKind::SymbolAddress,
            "local load reads from an explicit SymbolAddress");
    }

    {
        FrontendCompilation compilation;
        tests.expect(
            compile("long F() { int x = 1; return x; }", compilation),
            "return conversion sample builds HIR");
        const HirNode* function = findFunction(compilation.hirRoot(), "F");
        const HirNode* ret = findFirst(function, HirNodeKind::Return);
        tests.expect(
            ret != nullptr && ret->first() != nullptr && ret->first()->kind() == HirNodeKind::Conversion,
            "implicit return conversion becomes a HIR Conversion node");
        tests.expect(
            ret != nullptr && ret->first() != nullptr &&
                ret->first()->conversionKind() == ConversionKind::ImplicitNumeric &&
                ret->first()->type()->kind() == TypeKind::Long,
            "return conversion records kind and destination type");
    }

    {
        FrontendCompilation compilation;
        tests.expect(
            compile("int Main() { byte b = 1; b += 2; return b; }", compilation),
            "compound assignment sample builds HIR");
        const HirNode* compound = findFirst(compilation.hirRoot(), HirNodeKind::CompoundAssignment);
        tests.expect(
            compound != nullptr && compound->first() != nullptr &&
                compound->first()->kind() == HirNodeKind::SymbolAddress,
            "compound assignment stores one explicit lvalue address");
        tests.expect(
            compound != nullptr && compound->hasReadConversion() &&
                compound->readConversionKind() == ConversionKind::IntegralPromotion,
            "compound assignment preserves the promotion of the loaded left value");
        tests.expect(
            compound != nullptr && compound->hasResultConversion() &&
                compound->resultConversionKind() == ConversionKind::ExplicitNumeric,
            "compound assignment preserves conversion back to the storage type");
    }

    {
        FrontendCompilation compilation;
        tests.expect(
            compile("int Main() { int[4] a; int i = 1; return a[i]; }", compilation),
            "fixed-array indexing sample builds HIR");
        const HirNode* index = findFirst(compilation.hirRoot(), HirNodeKind::IndexAddress);
        tests.expect(index != nullptr && index->requiresBoundsCheck(), "fixed-array index retains bounds-check duty");
        tests.expect(
            index != nullptr && index->operationType() != nullptr &&
                index->operationType()->kind() == TypeKind::Array && index->operationType()->count() == 4,
            "index address retains the semantic fixed-array base type");
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "struct P { byte A; int B; } "
            "int Main() { P p; p.B = 42; return p.B; }";
        tests.expect(compile(source, compilation), "struct field sample builds HIR");
        const HirNode* field = findFirst(compilation.hirRoot(), HirNodeKind::FieldAddress);
        tests.expect(
            field != nullptr && field->symbol() != nullptr && field->symbol()->name() == "B" &&
                field->hasOffset() && field->offset() == 4,
            "HIR field address carries the physical field offset from Sema/data layout");
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "extern C int puts(char* text); "
            "int Main() { puts(\"ok\"); return 0; }";
        tests.expect(compile(source, compilation), "extern C string-call sample builds HIR");
        const HirNode* call = findFirst(compilation.hirRoot(), HirNodeKind::Call);
        tests.expect(
            call != nullptr && call->symbol() != nullptr && call->symbol()->name() == "puts" &&
                call->children().size() == 1,
            "direct call carries the resolved function symbol");
        tests.expect(
            call != nullptr && !call->children().empty() &&
                call->children().front()->kind() == HirNodeKind::Conversion &&
                call->children().front()->conversionKind() == ConversionKind::ImplicitStringLiteralToCharPointer,
            "string literal C-interop conversion is explicit in HIR");
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "int Main() { int i = 0; nuint n = sizeof(i++); return (int)n; }";
        tests.expect(compile(source, compilation), "sizeof unevaluated operand sample builds HIR");
        const HirNode* local = findFirst(compilation.hirRoot(), HirNodeKind::LocalVariableDeclaration);
        const HirNode* sizeofConstant = nullptr;
        const HirNode* function = findFunction(compilation.hirRoot(), "Main");
        if (function != nullptr && function->first() != nullptr && function->first()->children().size() >= 2)
        {
            const HirNode* nDeclaration = function->first()->children()[1];
            sizeofConstant = nDeclaration->first();
        }
        tests.expect(local != nullptr, "sizeof sample contains local declarations");
        tests.expect(
            sizeofConstant != nullptr && sizeofConstant->kind() == HirNodeKind::Constant &&
                sizeofConstant->type()->kind() == TypeKind::NUInt,
            "sizeof lowers directly to a nuint constant");
        const HirNode* increment = findFirst(sizeofConstant, HirNodeKind::Increment);
        tests.expect(increment == nullptr, "sizeof HIR does not evaluate or retain side-effecting operand execution");
    }

    {
        FrontendCompilation compilation;
        tests.expect(
            compile("int Main() { delete null; delete[] null; return 0; }", compilation),
            "delete-null forms build HIR");
        const HirNode* deallocation = findFirst(compilation.hirRoot(), HirNodeKind::Deallocation);
        tests.expect(
            deallocation != nullptr && deallocation->first() != nullptr &&
                deallocation->first()->type()->kind() == TypeKind::NullLiteral,
            "delete null preserves NullLiteralType without inventing a pointee type");
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "int Main() { int* h = new int[4]; int* s = stackalloc int[4]; delete[] h; return 0; }";
        tests.expect(compile(source, compilation), "allocation sample builds HIR");
        const HirNode* heap = findFirst(compilation.hirRoot(), HirNodeKind::Allocation);
        const HirNode* stack = findFirst(compilation.hirRoot(), HirNodeKind::StackAllocation);
        tests.expect(
            heap != nullptr && heap->arrayForm() && heap->operationType()->kind() == TypeKind::Int,
            "new[] records array form and element type");
        tests.expect(
            stack != nullptr && stack->operationType()->kind() == TypeKind::Int,
            "stackalloc records element type and count");
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "struct P { int X; } "
            "P Make() { P p; return p; } "
            "int Main() { return Make().X; }";
        tests.expect(compile(source, compilation), "rvalue struct member sample builds HIR");
        const HirNode* extract = findFirst(compilation.hirRoot(), HirNodeKind::ExtractField);
        tests.expect(
            extract != nullptr && extract->symbol() != nullptr && extract->symbol()->name() == "X" &&
                extract->first() != nullptr && extract->first()->kind() == HirNodeKind::Call,
            "field access on an rvalue struct uses ExtractField rather than an address/load pair");
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "int Main() { "
            "bool b = false && (1 / 0 == 0); "
            "if (b) { return 1; } "
            "return 0; "
            "}";
        tests.expect(compile(source, compilation), "short-circuit constant sample builds HIR");
        const HirNode* logicalAnd =
            findOperation(compilation.hirRoot(), HirNodeKind::Binary, TokenKind::AmpAmp);
        tests.expect(logicalAnd != nullptr, "HIR preserves logical AND as a semantic short-circuit operation");
        tests.expect(
            logicalAnd != nullptr &&
                findOperation(logicalAnd->second(), HirNodeKind::Binary, TokenKind::Slash) != nullptr,
            "HIR retains the unevaluated-at-compile-time RHS for later short-circuit CFG lowering");
    }

    return tests.result();
}
