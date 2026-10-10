#include "FrontendContractSupport.h"

#include "nativesharp/Frontend/ProjectMethodBodyModelDumper.h"

#include <sstream>
#include <string>

namespace nsc::suite
{
namespace
{
using namespace frontend_contract_support;
using namespace nativesharp::frontend;

std::string methodSource(std::size_t contract)
{
    switch (contract)
    {
        case 31:
            return "namespace Suite { public class C { public int F(int x) { return x + 1; } } }";
        case 32:
            return R"ns(namespace Suite { public class C { public int F(int x) {
                int value = x; { int nested = value + 1; value = nested; } return value; } } })ns";
        case 33:
            return R"ns(namespace Suite { public class Resource { public int value; }
                public class C { public int F(owned Resource input) {
                    owned Resource local = move input; return local.value; } } })ns";
        case 34:
            return R"ns(namespace Suite { public class C { public int F(int x) {
                int result = 0; while (x > 0) { if (x == 2) { x = x - 1; continue; }
                result = result + x; x = x - 1; } return result; } } })ns";
        case 35:
            return R"ns(namespace Suite { public class C { public int F(int x) {
                try { if (x < 0) throw; return x; } finally { x = x + 1; } } } })ns";
        case 36:
            return R"ns(namespace Suite { public class C { public int F(int x, int y) {
                return (x + y) * (x - y) + (x > y ? x : y); } } })ns";
        case 37:
            return R"ns(namespace Suite { public class C { private int value;
                public int F() { this.value = value + 1; return this.value; } } })ns";
        case 38:
            return R"ns(namespace Suite { public class C { public int Add(int x, int y) { return x + y; }
                public int F() { return Add(2, 3); } } })ns";
        case 39:
            return R"ns(namespace Suite { public class C { public int F(int x) {
                int shifted = (x + 1) << 2; return ~shifted ^ (x * 3); } } })ns";
        case 40:
            return R"ns(namespace Suite { public struct Pair { public int left; public int right; }
                public class C { private int value; public int F(Pair pair) {
                    value += pair.left; pair.right = value; return pair.right; } } })ns";
        case 41:
            return R"ns(namespace Suite { public class Base { } public class Derived : Base { }
                public class C { public Base F(Derived value) { return value; } } })ns";
        case 42:
            return R"ns(namespace Suite { public class Resource { public int value; }
                public class C { public int Consume(owned Resource value) { return value.value; }
                public int F(owned Resource source) { return Consume(move source); } } })ns";
        case 43:
            return R"ns(namespace Suite { public delegate int Transform(int value);
                public class C { public Transform Make(int seed) { return x => x + seed; }
                public int F() { Transform transform = Make(2); return transform(3); } } })ns";
        case 44:
            return R"ns(namespace Suite { public class C { public int F(int x) {
                int value = x; value = value + 1; return value; } } })ns";
        default:
            return "namespace Suite { public class C { public int F() { return 1; } } }";
    }
}

void inspectMethodContract(
    TestContext& context, std::size_t contract, const ProjectMethodBodyModel& model)
{
    context.expect(model.isBuilt(), "method-body stage must publish one completed semantic model");
    switch (contract)
    {
        case 31:
            context.expect(model.callableAnalysisStatistics().callablesAnalyzed != 0,
                "method-body waves must account for analyzed callables");
            break;
        case 32:
            context.expect(!model.scopes().empty() && !model.locals().empty(),
                "scope and definite-assignment analysis must retain scopes and locals");
            break;
        case 33:
            context.expect(!model.moveBindings().empty() || !model.ownershipTransferBindings().empty(),
                "ownership flow must retain a move or transfer binding");
            break;
        case 34:
            context.expect(!model.structuredExits().empty(),
                "statement analysis must retain structured loop exits");
            break;
        case 35:
            context.expect(!model.exceptionBindings().empty() || !model.cleanupRegions().empty(),
                "exception analysis must retain an exception or cleanup region");
            break;
        case 36:
            context.expect(!model.expressionTypes().empty(),
                "expression traversal must publish contextual type bindings");
            break;
        case 37:
            context.expect(!model.nameBindings().empty(),
                "identifier/member analysis must retain exact selected name bindings");
            break;
        case 38:
            context.expect(!model.calls().empty(),
                "call analysis must retain argument mapping and selected target");
            break;
        case 39:
            context.expect(!model.expressionTypes().empty(),
                "numeric operator analysis must retain result type bindings");
            break;
        case 40:
            context.expect(!model.nameBindings().empty() && !model.expressionTypes().empty(),
                "assignment analysis must retain target and value typing");
            break;
        case 41:
            context.expect(!model.expressionTypes().empty(),
                "cast and carrier analysis must retain source and target types");
            break;
        case 42:
            context.expect(!model.moveBindings().empty() || !model.ownershipTransferBindings().empty(),
                "ownership analysis must retain explicit transfer state");
            break;
        case 43:
            context.expect(!model.lambdaBindings().empty() && !model.delegateBindings().empty(),
                "lambda analysis must retain captures and target delegate binding");
            break;
        case 44:
            context.expect(model.coreBindings().empty(),
                "ordinary arithmetic must not be misclassified as a Core intrinsic");
            break;
    }
}

void methodContractOracle(
    TestContext& context, std::size_t variant, std::size_t contract)
{
    expectProjectStage(context, contract, variant, ProjectStage::MethodBodies, methodSource(contract),
        [contract](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(compilation.methodBodyModel() != nullptr,
                "method-body stage must publish ProjectMethodBodyModel");
            if (compilation.methodBodyModel() != nullptr)
            {
                inspectMethodContract(test, contract, *compilation.methodBodyModel());
            }
        });
}

void methodDumpOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 45, variant, ProjectStage::MethodBodies, methodSource(38),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            ProjectMethodBodyModelDumper dumper;
            std::ostringstream first;
            std::ostringstream second;
            dumper.dump(*compilation.methodBodyModel(), *compilation.memberModel(),
                *compilation.nameLookup(), first);
            dumper.dump(*compilation.methodBodyModel(), *compilation.memberModel(),
                *compilation.nameLookup(), second);
            test.expect(first.str() == second.str(), "method-body dump must be byte deterministic");
            test.expect(first.str().find("Add") != std::string::npos,
                "method-body dump must retain declared callable identity");
        });
}
}

void registerFrontendMethodBodiesModule(TestRegistry& registry)
{
    addMatrix(registry, 31, "component", [](TestContext& c, std::size_t v) { methodContractOracle(c, v, 31); });
    addMatrix(registry, 32, "component", [](TestContext& c, std::size_t v) { methodContractOracle(c, v, 32); });
    addMatrix(registry, 33, "component", [](TestContext& c, std::size_t v) { methodContractOracle(c, v, 33); });
    addMatrix(registry, 34, "component", [](TestContext& c, std::size_t v) { methodContractOracle(c, v, 34); });
    addMatrix(registry, 35, "component", [](TestContext& c, std::size_t v) { methodContractOracle(c, v, 35); });
    addMatrix(registry, 36, "component", [](TestContext& c, std::size_t v) { methodContractOracle(c, v, 36); });
    addMatrix(registry, 37, "component", [](TestContext& c, std::size_t v) { methodContractOracle(c, v, 37); });
    addMatrix(registry, 38, "component", [](TestContext& c, std::size_t v) { methodContractOracle(c, v, 38); });
    addMatrix(registry, 39, "component", [](TestContext& c, std::size_t v) { methodContractOracle(c, v, 39); });
    addMatrix(registry, 40, "component", [](TestContext& c, std::size_t v) { methodContractOracle(c, v, 40); });
    addMatrix(registry, 41, "component", [](TestContext& c, std::size_t v) { methodContractOracle(c, v, 41); });
    addMatrix(registry, 42, "component", [](TestContext& c, std::size_t v) { methodContractOracle(c, v, 42); });
    addMatrix(registry, 43, "component", [](TestContext& c, std::size_t v) { methodContractOracle(c, v, 43); });
    addMatrix(registry, 44, "component", [](TestContext& c, std::size_t v) { methodContractOracle(c, v, 44); });
    addMatrix(registry, 45, "unit", methodDumpOracle);
}
}
