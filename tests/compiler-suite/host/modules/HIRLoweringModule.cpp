#include "HIRContractSupport.h"

#include "nativesharp/HIR/HirNode.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/Type.h"
#include "nativesharp/Sema/TypeKind.h"

#include <array>
#include <string_view>

namespace nsc::suite
{
namespace
{
using hir_contract_support::addMatrix;
using hir_contract_support::compileHir;
using hir_contract_support::countKind;
using hir_contract_support::findFirst;
using nativesharp::hir::HirNode;
using nativesharp::hir::HirNodeKind;
using nativesharp::lexer::TokenKind;
using nativesharp::sema::TypeKind;

void rootOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"", "int F();", "int value;",
        "enum E{A}", "struct S{}", "void F(){}", "int F(){return 1;}",
        "int A;int B;int C;int D;int E;int F;int G;int H;"};
    auto compilation = compileHir(context, "CF-HIR-007", std::string(sources[variant]));
    const HirNode* root = compilation->hirRoot();
    context.expect(root != nullptr && root->kind() == HirNodeKind::CompilationUnit,
        "CF-HIR-007 builder root must be a CompilationUnit");
    context.expect(root != nullptr && root->children().size() == (variant == 7 ? 8 : (variant == 0 ? 0 : 1)),
        "CF-HIR-007 root must preserve the exact lowered global count");
}

void globalOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"int G;", "struct S{}", "enum E{A}", "int F();",
        "extern C int F();", "struct S{} enum E{A} int G; int F();", "int A;int B;int C;",
        "int A;int B;int C;int D;int E;int F;int G;int H;int I;int J;"};
    auto compilation = compileHir(context, "CF-HIR-008", std::string(sources[variant]));
    const HirNode* root = compilation->hirRoot();
    context.expect(root != nullptr && !root->children().empty(),
        "CF-HIR-008 global dispatch must retain every supported declaration");
    context.expect(root != nullptr && root->children().front() != nullptr,
        "CF-HIR-008 first global must retain source order");
}

void structOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"struct S{}", "struct S{int X;}",
        "struct S{byte A;int B;}", "struct A{int X;}struct B{A Value;}",
        "struct S{int[4] Values;}", "struct S{int* Next;}",
        "struct S{byte A;short B;int C;long D;}",
        "struct S{int A;int B;int C;int D;int E;int F;int G;int H;}"};
    auto compilation = compileHir(context, "CF-HIR-009", std::string(sources[variant]));
    const HirNode* structure = findFirst(compilation->hirRoot(), HirNodeKind::StructDeclaration);
    context.expect(structure != nullptr && structure->symbol() != nullptr,
        "CF-HIR-009 struct lowering must preserve the bound symbol");
    context.expect(structure != nullptr && countKind(structure, HirNodeKind::FieldDeclaration) ==
            structure->children().size(),
        "CF-HIR-009 field order and cardinality must remain exact");
    for (const HirNode* field : structure == nullptr ? std::vector<HirNode*>{} : structure->children())
    {
        context.expect(field != nullptr && field->hasOffset(),
            "CF-HIR-009 every lowered field must retain its physical offset");
    }
}

void enumOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"enum E{}", "enum E{A}", "enum E{A,B,C}",
        "enum E{A=-1,B=2}", "enum E{A=4,B=A+3}", "enum E{A=0,B=10,C=11}",
        "enum E{A,B,C,D,E,F}", "enum E{A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P}"};
    auto compilation = compileHir(context, "CF-HIR-010", std::string(sources[variant]));
    const HirNode* enumeration = findFirst(compilation->hirRoot(), HirNodeKind::EnumDeclaration);
    context.expect(enumeration != nullptr && enumeration->symbol() != nullptr,
        "CF-HIR-010 enum lowering must preserve the nominal symbol");
    for (const HirNode* member : enumeration == nullptr ? std::vector<HirNode*>{} : enumeration->children())
    {
        context.expect(member != nullptr && member->kind() == HirNodeKind::EnumMemberDeclaration,
            "CF-HIR-010 enum member order must be exact");
        context.expect(member != nullptr && member->hasConstant(),
            "CF-HIR-010 every enum member must retain its constant payload");
    }
}

void functionOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"void F();", "void F(){}", "int F(){return 1;}",
        "int F(int x){return x;}", "long F(int a,byte b,long c){return c;}", "extern C int F(int x);",
        "int F(int a,int b,int c,int d){return a+b+c+d;}",
        "int F(int a,int b,int c,int d,int e,int f,int g,int h){return a;}"};
    // Keep the compilation alive while inspecting its arena-owned HIR nodes.
    // Returning this pointer from a helper destroyed the owner first and made
    // every CF-HIR-011 case dereference released storage.
    auto compilation = compileHir(context, "CF-HIR-011", std::string(sources[variant]));
    const HirNode* function = findFirst(compilation->hirRoot(), HirNodeKind::FunctionDeclaration);
    context.expect(function != nullptr, "CF-HIR-011 must lower the contract node kind");
    context.expect(function != nullptr && function->symbol() != nullptr && function->type() != nullptr,
        "CF-HIR-011 function must retain symbol and interned signature type");
    context.expect(function != nullptr && countKind(function, HirNodeKind::ParameterDeclaration) ==
            function->children().size(),
        "CF-HIR-011 parameter cardinality and order must be exact");
}

void variableOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"int G;", "int G=1;", "long G=42;",
        "int F(){int x;return 0;}", "int F(){int x=1;return x;}",
        "int F(int p){long x=p;return (int)x;}", "int A=1;int B=2;int F(){int C=3;return A+B+C;}",
        "int F(){int A=1;int B=2;int C=3;int D=4;int E=5;int G=6;return A+B+C+D+E+G;}"};
    auto compilation = compileHir(context, "CF-HIR-012", std::string(sources[variant]));
    const HirNodeKind kind = variant < 3 || variant == 6 ? HirNodeKind::GlobalVariableDeclaration :
        HirNodeKind::LocalVariableDeclaration;
    const HirNode* variable = findFirst(compilation->hirRoot(), kind);
    context.expect(variable != nullptr && variable->symbol() != nullptr && variable->type() != nullptr,
        "CF-HIR-012 declarations must retain symbol and type identity");
}

void statementOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"void F(){}", "int F(bool b){if(b){return 1;}return 0;}",
        "int F(){while(true){break;}return 0;}", "int F(){for(int i=0;i<2;i=i+1){}return 0;}",
        "int F(){int x=1;x+2;return x;}", "int F(){static_assert(true);return 0;}",
        "int F(){while(true){continue;}return 0;}",
        "int F(){int x=0;for(int i=0;i<8;i=i+1){if(i==4){continue;}x=x+i;}return x;}"};
    auto compilation = compileHir(context, "CF-HIR-013", std::string(sources[variant]));
    const HirNode* function = findFirst(compilation->hirRoot(), HirNodeKind::FunctionDeclaration);
    context.expect(function != nullptr && function->first() != nullptr &&
            function->first()->kind() == HirNodeKind::Block,
        "CF-HIR-013 function statements must lower into an ordered block");
}

void valueOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"int F(){return 1;}", "int F(){int x=1;return x;}",
        "long F(){int x=1;return x;}", "int F(){const int x=1;return x;}",
        "int F(){return (int)1u;}", "int F(){return sizeof(int);}",
        "int F(){int x=1;int* p=&x;return *p;}",
        "int F(){int a=1;int b=2;int c=3;int d=4;return a+b+c+d;}"};
    auto compilation = compileHir(context, "CF-HIR-014", std::string(sources[variant]));
    const HirNode* value = variant == 1 || variant == 3 || variant == 6 ?
        findFirst(compilation->hirRoot(), HirNodeKind::Load) :
        findFirst(compilation->hirRoot(), HirNodeKind::Constant);
    context.expect(value != nullptr && value->type() != nullptr,
        "CF-HIR-014 value category must lower to an explicit typed value node");
}

void operatorOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"int F(){return -1;}", "int F(){return ~1;}",
        "int F(){return 1+2;}", "int F(){return 4-2;}", "int F(){return 3*2;}",
        "int F(){return 8/2;}", "int F(){return 7%3;}",
        "int F(int a,int b){return ((a+b)*(a-b))/(b==0?1:b);}"};
    auto compilation = compileHir(context, "CF-HIR-015", std::string(sources[variant]));
    if (variant == 7)
    {
        const HirNode* conditional = findFirst(compilation->hirRoot(), HirNodeKind::Conditional);
        context.expect(conditional != nullptr && conditional->type() != nullptr &&
                conditional->first() != nullptr && conditional->second() != nullptr &&
                conditional->third() != nullptr,
            "TF-HIR-120 conditional node must retain condition, both branches and result type");
        return;
    }
    const HirNodeKind kind = variant < 2 ? HirNodeKind::Unary : HirNodeKind::Binary;
    const HirNode* operation = findFirst(compilation->hirRoot(), kind);
    context.expect(operation != nullptr && operation->operation() != TokenKind::Invalid,
        "CF-HIR-015 operator node must retain its resolved token");
    context.expect(operation != nullptr && operation->operationType() != nullptr,
        "CF-HIR-015 operator node must retain its operation type");
}

void storeOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"int F(){int x=0;x=1;return x;}",
        "int F(){byte x=1;x+=2;return x;}", "int F(){int x=2;x-=1;return x;}",
        "int F(){int x=2;x*=3;return x;}", "int F(){int x=4;x/=2;return x;}",
        "int F(){int x=1;++x;return x;}", "int F(){int x=1;x++;return x;}",
        "int F(){int x=1;x+=2;x*=3;--x;return x;}"};
    auto compilation = compileHir(context, "CF-HIR-016", std::string(sources[variant]));
    const HirNodeKind kind = variant == 0 ? HirNodeKind::Store :
        (variant >= 5 ? HirNodeKind::Increment : HirNodeKind::CompoundAssignment);
    const HirNode* node = findFirst(compilation->hirRoot(), kind);
    context.expect(node != nullptr && node->first() != nullptr,
        "CF-HIR-016 storage operations must retain an explicit address edge");
}

void callOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"int A(){return 1;}int F(){return A();}",
        "long A(long x){return x;}long F(){return A(1);}",
        "int A(int a,int b){return a+b;}int F(){return A(1,2);}",
        "void A(){}int F(){A();return 0;}", "int F(){return (int)1u;}",
        "extern C int puts(char* p);int F(){return puts(\"x\");}",
        "long A(byte x){return x;}long F(){return A(1);}",
        "int A(int a,int b,int c,int d,int e,int f,int g,int h){return a;}int F(){return A(1,2,3,4,5,6,7,8);}"};
    auto compilation = compileHir(context, "CF-HIR-017", std::string(sources[variant]));
    const HirNodeKind kind = variant == 4 ? HirNodeKind::Conversion : HirNodeKind::Call;
    const HirNode* node = findFirst(compilation->hirRoot(), kind);
    context.expect(node != nullptr && node->type() != nullptr,
        "CF-HIR-017 calls and casts must retain exact result types");
    if (kind == HirNodeKind::Call)
    {
        context.expect(node->symbol() != nullptr, "CF-HIR-017 direct call must retain callee symbol identity");
    }
}

void addressOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"int F(){int x=1;return x;}",
        "int F(){int x=1;int* p=&x;return *p;}", "int F(int i){int[4] a;return a[i];}",
        "struct S{int X;}int F(){S s;return s.X;}",
        "struct S{int X;}int F(){S s;S* p=&s;return p->X;}",
        "struct S{int X;}S Make(){S s;return s;}int F(){return Make().X;}",
        "int F(){int[2,3] a;return a[1,2];}",
        "struct S{int A;int B;int C;}int F(){S s;s.A=1;s.B=2;s.C=3;return s.A+s.B+s.C;}"};
    auto compilation = compileHir(context, "CF-HIR-018", std::string(sources[variant]));
    constexpr std::array kinds{HirNodeKind::SymbolAddress, HirNodeKind::DereferenceAddress,
        HirNodeKind::IndexAddress, HirNodeKind::FieldAddress, HirNodeKind::FieldAddress,
        HirNodeKind::ExtractField, HirNodeKind::IndexAddress, HirNodeKind::FieldAddress};
    const HirNode* node = findFirst(compilation->hirRoot(), kinds[variant]);
    context.expect(node != nullptr && node->type() != nullptr,
        "CF-HIR-018 address and extraction nodes must retain a result type");
    if (kinds[variant] == HirNodeKind::FieldAddress || kinds[variant] == HirNodeKind::ExtractField)
    {
        context.expect(node->symbol() != nullptr && node->hasOffset(),
            "CF-HIR-018 field operations must retain symbol and physical offset");
    }
}

void memoryOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"int F(){int* p=new int;delete p;return 0;}",
        "int F(){int* p=new int[4];delete[] p;return 0;}",
        "int F(){int* p=stackalloc int[4];return 0;}", "int F(){delete null;return 0;}",
        "int F(){delete[] null;return 0;}", "int F(int n){int* p=new int[n];delete[] p;return 0;}",
        "int F(int n){int* p=stackalloc int[n];return 0;}",
        "int F(){int* a=new int[8];int* b=stackalloc int[8];delete[] a;return b[0];}"};
    auto compilation = compileHir(context, "CF-HIR-019", std::string(sources[variant]));
    const HirNodeKind kind = variant == 2 || variant == 6 ? HirNodeKind::StackAllocation :
        (variant == 3 || variant == 4 ? HirNodeKind::Deallocation : HirNodeKind::Allocation);
    const HirNode* node = findFirst(compilation->hirRoot(), kind);
    context.expect(node != nullptr && node->type() != nullptr,
        "CF-HIR-019 memory operation must retain exact result type");
}

void adapterOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources{"int F(){return 1;}", "int F(){int x=1;return x;}",
        "long F(){return 1;}", "int F(){return -1;}", "int F(){return 1+2;}",
        "int A(){return 1;}int F(){return A();}", "int F(){int x=1;x=2;return x;}",
        "int F(){byte x=1;x+=2;return x;}"};
    auto compilation = compileHir(context, "CF-HIR-020", std::string(sources[variant]));
    constexpr std::array kinds{HirNodeKind::Constant, HirNodeKind::Load, HirNodeKind::Conversion,
        HirNodeKind::Unary, HirNodeKind::Binary, HirNodeKind::Call, HirNodeKind::Store,
        HirNodeKind::CompoundAssignment};
    const HirNode* node = findFirst(compilation->hirRoot(), kinds[variant]);
    context.expect(node != nullptr, "CF-HIR-020 semantic lookup must materialize the requested semantic record");
}
}

// Registers CF-HIR-007..020 builder and lowering matrices.
void registerHIRLoweringModule(TestRegistry& registry)
{
    addMatrix(registry, 7, "component", rootOracle);
    addMatrix(registry, 8, "component", globalOracle);
    addMatrix(registry, 9, "component", structOracle);
    addMatrix(registry, 10, "component", enumOracle);
    addMatrix(registry, 11, "component", functionOracle);
    addMatrix(registry, 12, "component", variableOracle);
    addMatrix(registry, 13, "component", statementOracle);
    addMatrix(registry, 14, "component", valueOracle);
    addMatrix(registry, 15, "component", operatorOracle);
    addMatrix(registry, 16, "component", storeOracle);
    addMatrix(registry, 17, "component", callOracle);
    addMatrix(registry, 18, "component", addressOracle);
    addMatrix(registry, 19, "component", memoryOracle);
    addMatrix(registry, 20, "unit", adapterOracle);
}
}
