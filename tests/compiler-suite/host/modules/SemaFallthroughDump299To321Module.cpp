#include "SemaConversionEvaluationTestSupport.h"

#include <span>

namespace nsc::suite
{
namespace
{
using namespace sema_conversion_evaluation_support;

void bindBoolean(SemaFixture& fixture, AstNode* node, bool value)
{
    fixture.analyzer.model_.set(node,
        booleanConstant(fixture.types.builtin(TypeKind::Bool), value));
}

std::string dumpSemantic(const AstNode* root, const SemanticModel& model)
{
    std::ostringstream output;
    SemanticDumper{}.dump(root, model, output);
    return output.str();
}

std::string dumpSymbols(std::span<Symbol* const> symbols)
{
    std::ostringstream output;
    SymbolDumper{}.dump(symbols, output);
    return output.str();
}

void testTerminalStatements(TestContext& context)
{
    SemaFixture fixture;
    context.expect(fixture.analyzer.statementPreventsFallthrough(
                fixture.node(AstNodeKind::ReturnStatement)) &&
            fixture.analyzer.statementPreventsFallthrough(fixture.node(AstNodeKind::BreakStatement)) &&
            fixture.analyzer.statementPreventsFallthrough(fixture.node(AstNodeKind::ContinueStatement)),
        "TS-SEMA-299 return, break and continue must each prevent syntactic fallthrough");
}

void testBlockFirstTerminator(TestContext& context)
{
    SemaFixture fixture;
    AstNode* empty = fixture.node(AstNodeKind::BlockStatement);
    AstNode* first = fixture.node(AstNodeKind::BlockStatement);
    first->addChild(fixture.node(AstNodeKind::ReturnStatement));
    first->addChild(fixture.node(AstNodeKind::ExpressionStatement));
    AstNode* middle = fixture.node(AstNodeKind::BlockStatement);
    middle->addChild(fixture.node(AstNodeKind::ExpressionStatement));
    middle->addChild(fixture.node(AstNodeKind::BreakStatement));
    middle->addChild(fixture.node(AstNodeKind::ExpressionStatement));
    AstNode* none = fixture.node(AstNodeKind::BlockStatement);
    none->addChild(fixture.node(AstNodeKind::ExpressionStatement));
    context.expect(!fixture.analyzer.statementPreventsFallthrough(empty) &&
            fixture.analyzer.statementPreventsFallthrough(first) &&
            fixture.analyzer.statementPreventsFallthrough(middle) &&
            !fixture.analyzer.statementPreventsFallthrough(none),
        "TS-SEMA-300 blocks must be dominated by the first terminating child regardless of later statements");
}

void testIfFallthroughMatrix(TestContext& context)
{
    SemaFixture fixture;
    auto makeIf = [&](AstNode* thenNode, AstNode* elseNode) {
        AstNode* node = fixture.node(AstNodeKind::IfStatement);
        node->setFirst(fixture.node(AstNodeKind::IdentifierExpression));
        node->setSecond(thenNode);
        node->setThird(elseNode);
        return node;
    };
    AstNode* noElse = makeIf(fixture.node(AstNodeKind::ReturnStatement), nullptr);
    AstNode* oneBranch = makeIf(
        fixture.node(AstNodeKind::ReturnStatement), fixture.node(AstNodeKind::ExpressionStatement));
    AstNode* both = makeIf(
        fixture.node(AstNodeKind::ReturnStatement), fixture.node(AstNodeKind::ContinueStatement));
    context.expect(!fixture.analyzer.statementPreventsFallthrough(noElse) &&
            !fixture.analyzer.statementPreventsFallthrough(oneBranch) &&
            fixture.analyzer.statementPreventsFallthrough(both),
        "TS-SEMA-301 dynamic if prevents fallthrough only when both present branches terminate");
}

void testInfiniteLoopsWithoutBreak(TestContext& context)
{
    SemaFixture fixture;
    AstNode* trueCondition = fixture.boolean(true);
    bindBoolean(fixture, trueCondition, true);
    AstNode* whileLoop = fixture.node(AstNodeKind::WhileStatement);
    whileLoop->setFirst(trueCondition);
    whileLoop->setSecond(fixture.node(AstNodeKind::BlockStatement));
    AstNode* forLoop = fixture.node(AstNodeKind::ForStatement);
    forLoop->addChild(fixture.node(AstNodeKind::BlockStatement));
    context.expect(fixture.analyzer.statementPreventsFallthrough(whileLoop) &&
            fixture.analyzer.statementPreventsFallthrough(forLoop),
        "TS-SEMA-302 constant-true while and conditionless for without breaks must prevent fallthrough");
}

void testFiniteAndDynamicLoops(TestContext& context)
{
    SemaFixture fixture;
    AstNode* falseCondition = fixture.boolean(false);
    bindBoolean(fixture, falseCondition, false);
    AstNode* falseLoop = fixture.node(AstNodeKind::WhileStatement);
    falseLoop->setFirst(falseCondition);
    falseLoop->setSecond(fixture.node(AstNodeKind::BlockStatement));
    AstNode* dynamicLoop = fixture.node(AstNodeKind::WhileStatement);
    dynamicLoop->setFirst(fixture.node(AstNodeKind::IdentifierExpression));
    dynamicLoop->setSecond(fixture.node(AstNodeKind::BlockStatement));
    context.expect(!fixture.analyzer.statementPreventsFallthrough(falseLoop) &&
            !fixture.analyzer.statementPreventsFallthrough(dynamicLoop),
        "TS-SEMA-303 constant-false and dynamic loops must not prevent fallthrough");
}

void testBreakInLoopBody(TestContext& context)
{
    SemaFixture fixture;
    auto loopWithBody = [&](AstNode* body) {
        AstNode* condition = fixture.boolean(true);
        bindBoolean(fixture, condition, true);
        AstNode* loop = fixture.node(AstNodeKind::WhileStatement);
        loop->setFirst(condition);
        loop->setSecond(body);
        return loop;
    };
    AstNode* direct = fixture.node(AstNodeKind::BreakStatement);
    AstNode* block = fixture.node(AstNodeKind::BlockStatement);
    block->addChild(fixture.node(AstNodeKind::BreakStatement));
    AstNode* branch = fixture.node(AstNodeKind::IfStatement);
    branch->setFirst(fixture.node(AstNodeKind::IdentifierExpression));
    branch->setSecond(fixture.node(AstNodeKind::BreakStatement));
    branch->setThird(fixture.node(AstNodeKind::ExpressionStatement));
    context.expect(!fixture.analyzer.statementPreventsFallthrough(loopWithBody(direct)) &&
            !fixture.analyzer.statementPreventsFallthrough(loopWithBody(block)) &&
            !fixture.analyzer.statementPreventsFallthrough(loopWithBody(branch)),
        "TS-SEMA-304 direct, block and conditional breaks targeting a loop must make that loop fall through");
}

void testNestedLoopBreakIsolation(TestContext& context)
{
    SemaFixture fixture;
    AstNode* innerCondition = fixture.boolean(true);
    bindBoolean(fixture, innerCondition, true);
    AstNode* inner = fixture.node(AstNodeKind::WhileStatement);
    inner->setFirst(innerCondition);
    inner->setSecond(fixture.node(AstNodeKind::BreakStatement));
    AstNode* outerBody = fixture.node(AstNodeKind::BlockStatement);
    outerBody->addChild(inner);
    AstNode* outerCondition = fixture.boolean(true);
    bindBoolean(fixture, outerCondition, true);
    AstNode* outer = fixture.node(AstNodeKind::WhileStatement);
    outer->setFirst(outerCondition);
    outer->setSecond(outerBody);
    context.expect(fixture.analyzer.statementPreventsFallthrough(outer),
        "TS-SEMA-305 a break in a nested loop must not count as a break targeting the outer loop");
}

void testDeepAcyclicFallthroughBoundary(TestContext& context)
{
    SemaFixture fixture;
    AstNode* root = fixture.node(AstNodeKind::BlockStatement);
    AstNode* current = root;
    for (int depth = 0; depth < 256; ++depth)
    {
        AstNode* child = fixture.node(AstNodeKind::BlockStatement);
        current->addChild(child);
        current = child;
    }
    current->addChild(fixture.node(AstNodeKind::ReturnStatement));
    context.expect(fixture.analyzer.statementPreventsFallthrough(root),
        "TS-SEMA-306 a 256-level acyclic block must propagate its terminal statement");
    context.expect(false,
        "TS-SEMA-306 TEST-INFRASTRUCTURE: the unbounded recursion limit must be characterized in a crash-isolated process");
}

void testSemanticDumpEmpty(TestContext& context)
{
    SemaFixture fixture;
    AstNode* unannotated = fixture.node(AstNodeKind::IdentifierExpression);
    context.expect(dumpSemantic(nullptr, fixture.analyzer.model()).empty() &&
            dumpSemantic(unannotated, fixture.analyzer.model()).empty(),
        "TS-SEMA-307 null root and an unannotated tree over an empty model must produce exact empty output");
}

void testSemanticDumpMinimalExpression(TestContext& context)
{
    SemaFixture fixture;
    AstNode* child = fixture.integer(7);
    AstNode* root = fixture.node(AstNodeKind::ExpressionStatement);
    root->setFirst(child);
    fixture.analyzer.model_.set(child, signedConstant(fixture.types.builtin(TypeKind::Int), 7));
    const std::string expected = "  first: IntegerLiteralExpression \"7\" : int value constant=7\n";
    context.expect(dumpSemantic(root, fixture.analyzer.model()) == expected,
        "TS-SEMA-308 minimal ExpressionInfo dump must preserve exact edge, indent and format");
}

void testSemanticDumpCategoriesAndFlags(TestContext& context)
{
    SemaFixture fixture;
    const std::array categories = {ValueCategory::Value, ValueCategory::Place, ValueCategory::Temporary,
        ValueCategory::BorrowedValue, ValueCategory::MovedFrom};
    bool exact = true;
    for (ValueCategory category : categories)
    {
        AstNode* node = fixture.node(AstNodeKind::IdentifierExpression);
        node->setText("value");
        ExpressionInfo info;
        info.type = fixture.types.builtin(TypeKind::Int);
        info.category = category;
        info.modifiable = true;
        info.addressable = true;
        info.requiresBoundsCheck = true;
        fixture.analyzer.model_.set(node, info);
        const std::string expected = "IdentifierExpression \"value\" : int " +
            std::string(nativesharp::sema::valueCategoryName(category)) +
            " modifiable addressable bounds-check\n";
        exact = exact && dumpSemantic(node, fixture.analyzer.model()) == expected;
    }
    context.expect(exact, "TS-SEMA-309 every ValueCategory and expression flag must retain exact dump spelling");
}

void testSemanticDumpConstants(TestContext& context)
{
    SemaFixture fixture;
    struct ConstantExpectation
    {
        ConstantValue value;
        Type* type;
        std::string text;
    };
    const std::vector<ConstantExpectation> values = {
        {ConstantValue{}, fixture.types.builtin(TypeKind::Int), "<none>"},
        {ConstantValue::signedInteger(-2), fixture.types.builtin(TypeKind::Int), "-2"},
        {ConstantValue::unsignedInteger(3), fixture.types.builtin(TypeKind::UInt), "3"},
        {ConstantValue::floating(1.5), fixture.types.builtin(TypeKind::Double), "1.5"},
        {ConstantValue::boolean(true), fixture.types.builtin(TypeKind::Bool), "true"},
        {ConstantValue::nullValue(), fixture.types.builtin(TypeKind::NullLiteral), "null"},
        {ConstantValue::stringValue("abc"), fixture.types.stringLiteral(4), "\"abc\""}};
    bool exact = true;
    for (const ConstantExpectation& item : values)
    {
        AstNode* node = fixture.node(AstNodeKind::IdentifierExpression);
        ExpressionInfo info;
        info.type = item.type;
        info.isConstant = true;
        info.constantValue = item.value;
        fixture.analyzer.model_.set(node, info);
        const std::string expected = "IdentifierExpression : " + item.type->displayName() +
            " value constant=" + item.text + "\n";
        exact = exact && dumpSemantic(node, fixture.analyzer.model()) == expected;
    }
    context.expect(exact, "TS-SEMA-310 all seven ConstantKind payloads must have exact semantic dump representations");
}

void testSemanticDumpConversions(TestContext& context)
{
    SemaFixture fixture;
    AstNode* node = fixture.node(AstNodeKind::CastExpression);
    Type* byteType = fixture.types.builtin(TypeKind::Byte);
    Type* intType = fixture.types.builtin(TypeKind::Int);
    Type* longType = fixture.types.builtin(TypeKind::Long);
    fixture.analyzer.model_.addConversion(node,
        ConversionInfo{ConversionKind::IntegralPromotion, ConversionRole::Contextual, node, byteType, intType, -1});
    fixture.analyzer.model_.addConversion(node,
        ConversionInfo{ConversionKind::ImplicitNumeric, ConversionRole::Operand, node, intType, longType, 2});
    fixture.analyzer.model_.addConversion(node,
        ConversionInfo{ConversionKind::ExplicitNumeric, ConversionRole::Result, node, longType, intType, -1});
    const std::string expected =
        "CastExpression\n"
        "  conversion integral-promotion byte -> int contextual\n"
        "  conversion implicit-numeric int -> long operand=2\n"
        "  conversion explicit-numeric long -> int result\n";
    context.expect(dumpSemantic(node, fixture.analyzer.model()) == expected,
        "TS-SEMA-311 conversion dumps must preserve insertion order, roles, indices and exact fields");
}

void testSemanticDumpOperators(TestContext& context)
{
    SemaFixture fixture;
    AstNode* unary = fixture.node(AstNodeKind::UnaryExpression);
    AstNode* binary = fixture.node(AstNodeKind::BinaryExpression);
    Type* integer = fixture.types.builtin(TypeKind::Int);
    Type* boolean = fixture.types.builtin(TypeKind::Bool);
    fixture.analyzer.model_.setOperator(unary,
        nativesharp::sema::ResolvedOperatorInfo{TokenKind::Minus, integer, nullptr, integer, integer, true});
    fixture.analyzer.model_.setOperator(binary,
        nativesharp::sema::ResolvedOperatorInfo{TokenKind::Less, integer, integer, integer, boolean, false});
    context.expect(dumpSemantic(unary, fixture.analyzer.model()) ==
            "UnaryExpression\n  operator Minus operation=int result=int\n" &&
            dumpSemantic(binary, fixture.analyzer.model()) ==
            "BinaryExpression\n  operator Less operation=int result=bool\n",
        "TS-SEMA-312 unary and binary operator dumps must preserve exact token and operation/result types");
}

void testSemanticDumpTraversalAndOmission(TestContext& context)
{
    SemaFixture fixture;
    AstNode* root = fixture.node(AstNodeKind::BlockStatement);
    AstNode* first = fixture.node(AstNodeKind::IdentifierExpression);
    first->setText("first");
    AstNode* omitted = fixture.node(AstNodeKind::IdentifierExpression);
    omitted->setText("omitted");
    AstNode* nested = fixture.node(AstNodeKind::IdentifierExpression);
    nested->setText("nested");
    omitted->setFirst(nested);
    root->addChild(first);
    root->addChild(omitted);
    fixture.analyzer.model_.set(first, signedConstant(fixture.types.builtin(TypeKind::Int), 1));
    fixture.analyzer.model_.set(nested, signedConstant(fixture.types.builtin(TypeKind::Int), 2));
    const std::string output = dumpSemantic(root, fixture.analyzer.model());
    context.expect(output ==
            "  child: IdentifierExpression \"first\" : int value constant=1\n"
            "    first: IdentifierExpression \"nested\" : int value constant=2\n" &&
            output.find("omitted") == std::string::npos,
        "TS-SEMA-313 traversal must retain annotated descendants while omitting unannotated node records");
}

void testSemanticDumpRobustnessBoundary(TestContext& context)
{
    context.expect(false,
        "TS-SEMA-314 TEST-INFRASTRUCTURE: deep/cyclic SemanticDumper graphs require crash isolation because the product has no depth/visited guard");
}

void testSymbolDumpEmpty(TestContext& context)
{
    std::vector<Symbol*> symbols;
    context.expect(dumpSymbols(symbols).empty(),
        "TS-SEMA-315 an empty symbol span must produce exact empty output");
}

void testSymbolDumpKinds(TestContext& context)
{
    SemaFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::Int);
    bool exact = true;
    for (SymbolKind kind : {SymbolKind::Variable, SymbolKind::GlobalVariable, SymbolKind::Parameter,
             SymbolKind::Function, SymbolKind::Struct, SymbolKind::Enum, SymbolKind::Field, SymbolKind::EnumMember})
    {
        Symbol symbol(kind, "item", integer, nullptr);
        std::vector<Symbol*> symbols{&symbol};
        std::string expected = std::string(nativesharp::sema::symbolKindName(kind)) + " item : int";
        if (kind == SymbolKind::EnumMember) expected += " = 0";
        expected += "\n";
        exact = exact && dumpSymbols(symbols) == expected;
    }
    context.expect(exact, "TS-SEMA-316 every SymbolKind must retain its exact minimal dump format");
}

void testSymbolConditionalFields(TestContext& context)
{
    SemaFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::Int);
    Symbol function(SymbolKind::Function, "Run", integer, nullptr);
    function.setConst(true);
    function.setExternC(true);
    function.setDefined(true);
    Symbol enumerator(SymbolKind::EnumMember, "Choice", integer, nullptr);
    enumerator.setEnumValue(-4);
    Symbol field(SymbolKind::Field, "Value", integer, nullptr);
    field.setFieldOffset(8);
    Symbol constant(SymbolKind::GlobalVariable, "Answer", integer, nullptr);
    constant.setConstantValue(ConstantValue::signedInteger(42));
    std::vector<Symbol*> symbols{&function, &enumerator, &field, &constant};
    const std::string output = dumpSymbols(symbols);
    context.expect(output.find("Function Run : int const extern-C defined\n") != std::string::npos &&
            output.find("EnumMember Choice : int = -4\n") != std::string::npos &&
            output.find("Field Value : int offset=8\n") != std::string::npos,
        "TS-SEMA-317 flags, enum values and field offsets must appear only when present");
    context.expect(output.find("constant=42") != std::string::npos,
        "TS-SEMA-317 PRODUCT DEFECT: SymbolDumper must emit a symbol constant payload when present");
}

void testSymbolMemberOrdering(TestContext& context)
{
    SemaFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::Int);
    Symbol empty(SymbolKind::Struct, "Empty", integer, nullptr);
    Symbol one(SymbolKind::Struct, "One", integer, nullptr);
    Symbol single(SymbolKind::Field, "A", integer, nullptr);
    one.addMember(&single);
    Symbol many(SymbolKind::Struct, "Many", integer, nullptr);
    Symbol first(SymbolKind::Field, "First", integer, nullptr);
    Symbol second(SymbolKind::Field, "Second", integer, nullptr);
    Symbol third(SymbolKind::Field, "Third", integer, nullptr);
    many.addMember(&first);
    many.addMember(&second);
    many.addMember(&third);
    std::vector<Symbol*> symbols{&empty, &one, &many};
    context.expect(dumpSymbols(symbols) ==
            "Struct Empty : int\n"
            "Struct One : int\n"
            "  Field A : int\n"
            "Struct Many : int\n"
            "  Field First : int\n"
            "  Field Second : int\n"
            "  Field Third : int\n",
        "TS-SEMA-318 members 0/1/N must retain two-space indentation and insertion order");
}

void testSymbolGlobalOrdering(TestContext& context)
{
    SemaFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::Int);
    Symbol zeta(SymbolKind::GlobalVariable, "zeta", integer, nullptr);
    Symbol alpha(SymbolKind::GlobalVariable, "alpha", integer, nullptr);
    Symbol middle(SymbolKind::GlobalVariable, "middle", integer, nullptr);
    std::vector<Symbol*> symbols{&zeta, &alpha, &middle};
    context.expect(dumpSymbols(symbols) ==
            "GlobalVariable zeta : int\n"
            "GlobalVariable alpha : int\n"
            "GlobalVariable middle : int\n",
        "TS-SEMA-319 multiple globals must be dumped in the exact input-span order");
}

void testSymbolComplexNamesAndTypes(TestContext& context)
{
    SemaFixture fixture;
    Type* record = fixture.types.createStruct("NativeSharp.Tools.Pair", nullptr);
    Type* pointer = fixture.types.pointerTo(fixture.types.arrayOf(record, 3));
    Type* function = fixture.types.functionType(pointer,
        {fixture.types.builtin(TypeKind::Int), fixture.types.pointerTo(fixture.types.builtin(TypeKind::Char))});
    Symbol symbol(SymbolKind::Function, "Namespace::Type.Method<T>", function, nullptr);
    std::vector<Symbol*> symbols{&symbol};
    context.expect(dumpSymbols(symbols) ==
            "Function Namespace::Type.Method<T> : (int, char*) -> NativeSharp.Tools.Pair[3]*\n",
        "TS-SEMA-320 complex symbol names and types must use the exact Type::displayName text");
}

void testSymbolDumpRobustnessBoundary(TestContext& context)
{
    context.expect(false,
        "TS-SEMA-321 TEST-INFRASTRUCTURE: deep/cyclic Symbol member graphs require crash isolation because the dumper has no depth/visited guard");
}
}

void registerSemaFallthroughDump299To321Module(TestRegistry& registry)
{
    addSemaCase(registry, "TS-SEMA-299", "CS-SEMA-028", "unit", testTerminalStatements);
    addSemaCase(registry, "TS-SEMA-300", "CS-SEMA-028", "unit", testBlockFirstTerminator);
    addSemaCase(registry, "TS-SEMA-301", "CS-SEMA-028", "unit", testIfFallthroughMatrix);
    addSemaCase(registry, "TS-SEMA-302", "CS-SEMA-028", "unit", testInfiniteLoopsWithoutBreak);
    addSemaCase(registry, "TS-SEMA-303", "CS-SEMA-028", "unit", testFiniteAndDynamicLoops);
    addSemaCase(registry, "TS-SEMA-304", "CS-SEMA-028", "unit", testBreakInLoopBody);
    addSemaCase(registry, "TS-SEMA-305", "CS-SEMA-028", "unit", testNestedLoopBreakIsolation);
    addSemaCase(registry, "TS-SEMA-306", "CS-SEMA-028", "unit", testDeepAcyclicFallthroughBoundary,
        "NIGHTLY", TestDisposition::InfrastructurePending, {"infrastructure-special", "process-isolation"});
    addSemaCase(registry, "TS-SEMA-307", "CS-SEMA-029", "unit", testSemanticDumpEmpty);
    addSemaCase(registry, "TS-SEMA-308", "CS-SEMA-029", "unit", testSemanticDumpMinimalExpression);
    addSemaCase(registry, "TS-SEMA-309", "CS-SEMA-029", "unit", testSemanticDumpCategoriesAndFlags);
    addSemaCase(registry, "TS-SEMA-310", "CS-SEMA-029", "unit", testSemanticDumpConstants);
    addSemaCase(registry, "TS-SEMA-311", "CS-SEMA-029", "unit", testSemanticDumpConversions);
    addSemaCase(registry, "TS-SEMA-312", "CS-SEMA-029", "unit", testSemanticDumpOperators);
    addSemaCase(registry, "TS-SEMA-313", "CS-SEMA-029", "unit", testSemanticDumpTraversalAndOmission);
    addSemaCase(registry, "TS-SEMA-314", "CS-SEMA-029", "unit", testSemanticDumpRobustnessBoundary,
        "NIGHTLY", TestDisposition::InfrastructurePending, {"infrastructure-special", "process-isolation"});
    addSemaCase(registry, "TS-SEMA-315", "CS-SEMA-030", "unit", testSymbolDumpEmpty);
    addSemaCase(registry, "TS-SEMA-316", "CS-SEMA-030", "unit", testSymbolDumpKinds);
    addSemaCase(registry, "TS-SEMA-317", "CS-SEMA-030", "unit", testSymbolConditionalFields,
        "PR", TestDisposition::KnownProductDefect, {"product-defect", "missing-constant-field"});
    addSemaCase(registry, "TS-SEMA-318", "CS-SEMA-030", "unit", testSymbolMemberOrdering);
    addSemaCase(registry, "TS-SEMA-319", "CS-SEMA-030", "unit", testSymbolGlobalOrdering);
    addSemaCase(registry, "TS-SEMA-320", "CS-SEMA-030", "unit", testSymbolComplexNamesAndTypes);
    addSemaCase(registry, "TS-SEMA-321", "CS-SEMA-030", "unit", testSymbolDumpRobustnessBoundary,
        "NIGHTLY", TestDisposition::InfrastructurePending, {"infrastructure-special", "process-isolation"});
}
}
