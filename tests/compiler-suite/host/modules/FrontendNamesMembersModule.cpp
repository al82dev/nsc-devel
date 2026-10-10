#include "FrontendContractSupport.h"

#include "nativesharp/Frontend/ProjectInheritanceModelDumper.h"
#include "nativesharp/Frontend/ProjectInterfaceModelDumper.h"
#include "nativesharp/Frontend/ProjectMemberModelDumper.h"
#include "nativesharp/Frontend/ProjectNameLookupDumper.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModelDumper.h"

#include <sstream>
#include <string>

namespace nsc::suite
{
namespace
{
using namespace frontend_contract_support;
using namespace nativesharp::frontend;

std::string declarationsSource()
{
    return R"ns(
namespace Suite
{
    public interface IReadable { int Read(); }
    public class Base
    {
        protected int value;
        public Base(int source) { value = source; }
        public virtual int Read() { return value; }
    }
    public class Derived : Base, IReadable
    {
        public Derived(int source) : base(source) { }
        public override int Read() { return value + 1; }
    }
    public enum Mode : byte { None = 0, Live = 1 }
    public int Free(int value) { return value; }
}
)ns";
}

void namesOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 7, variant, ProjectStage::Names, declarationsSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(compilation.nameLookup() != nullptr, "name stage must publish ProjectNameLookup");
            test.expect(!compilation.units().empty(), "name lookup must retain its syntax units");
        });
}

void bindingsOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 8, variant, ProjectStage::Members, declarationsSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(compilation.nameLookup() != nullptr && compilation.memberModel() != nullptr,
                "bound declarations must retain both name and member models");
            test.expect(compilation.diagnostics().errorCount() == 0,
                "valid bindings must not publish diagnostics");
        });
}

void nameDumpOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 9, variant, ProjectStage::Names, declarationsSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            std::ostringstream first;
            std::ostringstream second;
            ProjectNameLookupDumper dumper;
            dumper.dump(*compilation.nameLookup(), first);
            dumper.dump(*compilation.nameLookup(), second);
            test.expect(first.str() == second.str(), "name lookup dump must be byte deterministic");
            test.expect(first.str().find("Suite") != std::string::npos,
                "name lookup dump must expose the declared namespace");
        });
}

void precompiledIdentityOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 10, variant, ProjectStage::Members, declarationsSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(compilation.memberModel() != nullptr,
                "member identity consumer must publish a member model");
            test.expect(compilation.nameLookup() != nullptr,
                "member identity consumer must retain canonical name ownership");
        });
}

void accessibilityOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 11, variant, ProjectStage::Inheritance, declarationsSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(compilation.memberModel() != nullptr && compilation.inheritanceModel() != nullptr,
                "accessibility requires member and inheritance contexts together");
            test.expect(compilation.diagnostics().errorCount() == 0,
                "valid protected/public access fixture must be accepted");
        });
}

void membersOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 12, variant, ProjectStage::Members, declarationsSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(compilation.memberModel() != nullptr, "member stage must publish ProjectMemberModel");
            std::ostringstream output;
            ProjectMemberModelDumper dumper;
            dumper.dump(*compilation.memberModel(), *compilation.nameLookup(), output);
            test.expect(output.str().find("Read") != std::string::npos,
                "member catalog must retain callable names and signatures");
        });
}

void ownershipMemberOracle(TestContext& context, std::size_t variant)
{
    const std::string source = R"ns(
namespace Suite
{
    public class Resource { public int value; }
    public class Holder
    {
        private owned Resource resource;
        public Holder(owned Resource value) { resource = move value; }
    }
}
)ns";
    expectProjectStage(context, 13, variant, ProjectStage::Members, source,
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(compilation.memberModel() != nullptr,
                "ownership-bearing members must remain in the member catalog");
            test.expect(compilation.diagnostics().errorCount() == 0,
                "valid ownership member declarations must be accepted");
        });
}

void enumMemberOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 14, variant, ProjectStage::Members,
        "namespace Suite { public enum Mode : byte { None = 0, One = 1, Two = 2 } }",
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(compilation.memberModel() != nullptr, "enum declarations must reach member modeling");
            std::ostringstream output;
            ProjectMemberModelDumper dumper;
            dumper.dump(*compilation.memberModel(), *compilation.nameLookup(), output);
            test.expect(output.str().find("Mode") != std::string::npos,
                "enum member dump must retain the declared enum identity");
        });
}

void memberDumpOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 15, variant, ProjectStage::Members, declarationsSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            ProjectMemberModelDumper dumper;
            std::ostringstream first;
            std::ostringstream second;
            dumper.dump(*compilation.memberModel(), *compilation.nameLookup(), first);
            dumper.dump(*compilation.memberModel(), *compilation.nameLookup(), second);
            test.expect(first.str() == second.str(), "member dump must be deterministic");
            test.expect(first.str().find("Derived") != std::string::npos,
                "member dump must retain declared type identity");
        });
}

void inheritanceOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 16, variant, ProjectStage::Inheritance, declarationsSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(compilation.inheritanceModel() != nullptr,
                "inheritance stage must publish ProjectInheritanceModel");
            std::ostringstream output;
            ProjectInheritanceModelDumper dumper;
            dumper.dump(*compilation.inheritanceModel(), *compilation.nameLookup(), output);
            test.expect(output.str().find("Derived") != std::string::npos,
                "inheritance dump must expose the derived type");
        });
}

void interfaceOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 17, variant, ProjectStage::Interfaces, declarationsSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(compilation.interfaceModel() != nullptr,
                "interface stage must publish ProjectInterfaceModel");
            std::ostringstream output;
            ProjectInterfaceModelDumper::dump(*compilation.interfaceModel(), output);
            test.expect(output.str().find("IReadable") != std::string::npos,
                "interface dump must retain contract identity");
        });
}

void dispatchOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 18, variant, ProjectStage::VirtualDispatch, declarationsSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(compilation.virtualDispatchModel() != nullptr,
                "dispatch stage must publish ProjectVirtualDispatchModel");
            std::ostringstream output;
            ProjectVirtualDispatchModelDumper dumper;
            dumper.dump(*compilation.virtualDispatchModel(), *compilation.memberModel(),
                *compilation.nameLookup(), output);
            test.expect(output.str().find("Read") != std::string::npos,
                "virtual dispatch dump must expose the overridden slot");
        });
}

void namespaceFunctionOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 19, variant, ProjectStage::Members,
        "namespace Suite { public int Add(int x, int y) { return x + y; } }",
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(compilation.namespaceFunctionModel() != nullptr,
                "member stage must publish namespace function identities");
            test.expect(compilation.diagnostics().errorCount() == 0,
                "valid namespace function must bind without diagnostics");
        });
}
}

void registerFrontendNamesMembersModule(TestRegistry& registry)
{
    addMatrix(registry, 7, "component", namesOracle);
    addMatrix(registry, 8, "component", bindingsOracle);
    addMatrix(registry, 9, "unit", nameDumpOracle);
    addMatrix(registry, 10, "unit", precompiledIdentityOracle);
    addMatrix(registry, 11, "unit", accessibilityOracle);
    addMatrix(registry, 12, "component", membersOracle);
    addMatrix(registry, 13, "component", ownershipMemberOracle);
    addMatrix(registry, 14, "component", enumMemberOracle);
    addMatrix(registry, 15, "unit", memberDumpOracle);
    addMatrix(registry, 16, "component", inheritanceOracle);
    addMatrix(registry, 17, "component", interfaceOracle);
    addMatrix(registry, 18, "component", dispatchOracle);
    addMatrix(registry, 19, "component", namespaceFunctionOracle);
}
}
