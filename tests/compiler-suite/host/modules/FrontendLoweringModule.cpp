#include "FrontendContractSupport.h"

#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"

#include <string>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace frontend_contract_support;
using namespace nativesharp::frontend;

std::string loweringSource()
{
    return R"ns(
namespace Suite
{
    public interface IValue { int Read(); }
    public struct Pair { public int left; public int right; }
    public class Base { public virtual int Read() { return 1; } }
    public class Item : Base, IValue
    {
        private int value;
        public Item(int source) { value = source; }
        public override int Read() { return value; }
        public int Sum(int count)
        {
            int total = 0;
            for (int index = 0; index < count; index = index + 1)
            {
                total = total + Read();
            }
            return total;
        }
    }
    public int Run() { Item value = new Item(3); return value.Sum(2); }
}
)ns";
}

std::size_t hirNodeCount(const ProjectOopHirModel& model)
{
    std::size_t count = 0;
    std::vector<const ProjectOopHirNode*> pending;
    for (const ProjectOopHirFunction& function : model.functions())
    {
        if (function.body != nullptr) pending.push_back(function.body.get());
    }
    while (!pending.empty())
    {
        const ProjectOopHirNode* node = pending.back();
        pending.pop_back();
        ++count;
        for (const auto& child : node->children)
        {
            if (child != nullptr) pending.push_back(child.get());
        }
    }
    return count;
}

std::size_t mirNodeCount(const ProjectOopMirModel& model)
{
    std::size_t count = 0;
    std::vector<const ProjectOopMirNode*> pending;
    for (const ProjectOopMirFunction& function : model.functions())
    {
        if (function.body != nullptr) pending.push_back(function.body.get());
    }
    while (!pending.empty())
    {
        const ProjectOopMirNode* node = pending.back();
        pending.pop_back();
        ++count;
        for (const auto& child : node->children)
        {
            if (child != nullptr) pending.push_back(child.get());
        }
    }
    return count;
}

void hirContractOracle(TestContext& context, std::size_t variant, std::size_t contract)
{
    expectProjectStage(context, contract, variant, ProjectStage::Hir, loweringSource(),
        [contract](TestContext& test, const ProjectCompilation& compilation)
        {
            const ProjectOopHirModel* model = compilation.oopHirModel();
            test.expect(model != nullptr && model->isBuilt(),
                "HIR stage must publish one built lowering model");
            if (model == nullptr) return;
            test.expect(!model->functions().empty(),
                "HIR lowering must retain every analyzed callable identity");
            const std::size_t nodes = hirNodeCount(*model);
            test.expect(nodes != 0, "HIR lowering must publish structured nodes for callable bodies");
            if (contract == 56)
            {
                test.expect(model->functions().size() >= 4,
                    "HIR merge must retain class, interface implementation and namespace functions");
            }
            else if (contract == 57)
            {
                test.expect(nodes > model->functions().size(),
                    "statement lowering must retain body control nodes below each function root");
            }
            else if (contract == 58)
            {
                test.expect(compilation.methodBodyModel() != nullptr &&
                        !compilation.methodBodyModel()->calls().empty(),
                    "HIR call lowering must consume an existing semantic call binding");
            }
            else if (contract == 59)
            {
                test.expect(compilation.constructionModel() != nullptr &&
                        !compilation.constructionModel()->constructions().empty(),
                    "HIR construction lowering must consume an existing construction plan");
            }
        });
}

void hirDumpOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 60, variant, ProjectStage::Hir, loweringSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            const std::string first = ProjectOopHirModelDumper::dump(*compilation.oopHirModel());
            const std::string second = ProjectOopHirModelDumper::dump(*compilation.oopHirModel());
            test.expect(first == second, "HIR dump must be byte deterministic");
            test.expect(first.find("Run") != std::string::npos,
                "HIR dump must retain namespace-function identity");
        });
}

void mirContractOracle(TestContext& context, std::size_t variant, std::size_t contract)
{
    expectProjectStage(context, contract, variant, ProjectStage::Mir, loweringSource(),
        [contract](TestContext& test, const ProjectCompilation& compilation)
        {
            const ProjectOopMirModel* model = compilation.oopMirModel();
            test.expect(model != nullptr && model->isBuilt(),
                "MIR stage must publish one built ABI-aware lowering model");
            if (model == nullptr) return;
            test.expect(!model->functions().empty() && mirNodeCount(*model) != 0,
                "MIR lowering must retain functions and physical nodes");
            if (contract == 61)
            {
                test.expect(!model->classLayouts().empty() && !model->structLayouts().empty(),
                    "MIR ABI model must retain class and struct physical layouts");
                test.expect(!model->interfaceBindings().empty(),
                    "MIR ABI model must retain class-to-interface bindings");
            }
            else
            {
                for (const ProjectOopMirFunction& function : model->functions())
                {
                    test.expect(!function.mangledName.empty(),
                        "every MIR function must preserve one physical symbol identity");
                }
            }
        });
}

void mirDumpOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 63, variant, ProjectStage::Mir, loweringSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            const std::string first = ProjectOopMirModelDumper::dump(*compilation.oopMirModel());
            const std::string second = ProjectOopMirModelDumper::dump(*compilation.oopMirModel());
            test.expect(first == second, "MIR dump must be byte deterministic");
            test.expect(first.find("Run") != std::string::npos,
                "MIR dump must retain namespace-function physical identity");
        });
}
}

void registerFrontendLoweringModule(TestRegistry& registry)
{
    addMatrix(registry, 56, "component", [](TestContext& c, std::size_t v) { hirContractOracle(c, v, 56); });
    addMatrix(registry, 57, "component", [](TestContext& c, std::size_t v) { hirContractOracle(c, v, 57); });
    addMatrix(registry, 58, "component", [](TestContext& c, std::size_t v) { hirContractOracle(c, v, 58); });
    addMatrix(registry, 59, "component", [](TestContext& c, std::size_t v) { hirContractOracle(c, v, 59); });
    addMatrix(registry, 60, "unit", hirDumpOracle);
    addMatrix(registry, 61, "component", [](TestContext& c, std::size_t v) { mirContractOracle(c, v, 61); });
    addMatrix(registry, 62, "component", [](TestContext& c, std::size_t v) { mirContractOracle(c, v, 62); });
    addMatrix(registry, 63, "unit", mirDumpOracle);
}
}
