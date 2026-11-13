#include "nativesharp/Frontend/ClassValueOrigins.h"
#include "nativesharp/Frontend/ClassReceiverEffectGraph.h"
#include "nativesharp/Frontend/ClassLocalScalarField.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "../support/TestContext.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>

namespace nativesharp::frontend
{
struct ProjectOopHirModelTestAccess final
{
    static bool constructionIndexUsesCallableIdentity()
    {
        ast::AstNode syntax(ast::AstNodeKind::VariableDeclarationStatement, {});
        ProjectConstructionModel model;
        model.constructions_.resize(3);
        model.constructions_[0].syntax = &syntax;
        model.constructions_[0].callerMemberOrdinal = 17;
        model.constructions_[1].syntax = &syntax;
        model.constructions_[1].callerMemberOrdinal = 29;
        model.constructions_[2].syntax = &syntax;
        model.constructions_[2].callerMemberOrdinal = 17;
        model.indexClassLocalConstruction(0);
        model.indexClassLocalConstruction(1);
        if (model.classLocalConstructionIndex_.latest(&syntax, 17) != 0 ||
            model.classLocalConstructionIndex_.latest(&syntax, 29) != 1 ||
            model.classLocalConstructionIndex_.latest(&syntax, 42).has_value() ||
            model.uniqueClassLocalConstruction(&syntax, 17) != &model.constructions_[0] ||
            model.uniqueClassLocalConstruction(&syntax, 29) != &model.constructions_[1] ||
            model.uniqueClassLocalConstruction(&syntax, 42) != nullptr) return false;
        model.indexClassLocalConstruction(2);
        if (model.classLocalConstructionIndex_.latest(&syntax, 17) != model.constructions_.size() ||
            model.classLocalConstructionIndex_.latest(&syntax, 29) != 1 ||
            model.uniqueClassLocalConstruction(&syntax, 17) != nullptr) return false;
        model.classLocalSafetyByOrdinal_.push_back(true);
        ProjectVirtualDispatchModel dispatch;
        model.setVirtualDispatchModel(&dispatch);
        return model.classLocalSafetyByOrdinal_.empty();
    }

    static bool finalizedReceiverEffects(const ProjectCompilation& project)
    {
        ProjectMethodBodyModel model;
        ProjectMethodBodyBuildOptions options;
        options.checkArrayReferenceDeadReceivers = false;
        diagnostics::DiagnosticEngine diagnostics;
        if (!model.build(*project.nameLookup(), *project.memberModel(), *project.namespaceFunctionModel(),
                *project.inheritanceModel(), *project.interfaceModel(), project.genericModel(), diagnostics,
                project.genericConstraintModel(), std::nullopt, options)) return false;
        model.classReceiverEffectSummaries_.resize(model.parameterEscapeSummaries_.size());
        model.finalizeClassReceiverEffects();
        bool safe = false, wrapper = false, unsafe = false, virtualCall = false, scalarCopy = false;
        const auto members = project.memberModel()->members();
        for (std::size_t i = 0; i < members.size(); ++i)
        {
            const auto effect = model.classReceiverEffectSummary(i);
            if (members[i].name == "Safe") safe = !effect.unknown && !effect.published;
            if (members[i].name == "Wrap") wrapper = !effect.unknown && !effect.published;
            if (members[i].name == "PublishAlias") unsafe = effect.unknown;
            if (members[i].name == "CallVirtual") virtualCall = effect.unknown;
            if (members[i].name == "SetValue") scalarCopy = !effect.unknown && !effect.published;
        }
        const auto checkLocal = [&](std::string_view methodName, bool expected)
        {
            for (std::size_t i = 0; i < model.locals().size(); ++i)
            {
                const auto& local = model.locals()[i];
                if (local.name != "q" || local.methodMemberOrdinal >= members.size() ||
                    members[local.methodMemberOrdinal].name != methodName) continue;
                const std::vector<std::size_t> ordinals{i};
                const auto result = model.classLocalUsesAreProvenSafe(ordinals);
                return result.size() == 1 && result[0] == expected;
            }
            return false;
        };
        if (!checkLocal("UseSafe", true) || !checkLocal("UseAlias", false) ||
            !checkLocal("UseUnknown", false)) return false;
        if (!safe || !wrapper || !unsafe || !virtualCall || !scalarCopy || project.constructionModel() == nullptr ||
            !project.constructionModel()->classConstructorReceiverAssessments().empty())
            return false;
        ProjectConstructionModel constructions;
        if (!constructions.build(*project.nameLookup(), *project.memberModel(),
                *project.inheritanceModel(), *project.interfaceModel(), model,
                *project.namespaceFunctionModel(), project.units(), diagnostics)) return false;
        const auto checkConstruction = [&](std::string_view methodName, bool expected)
        {
            for (std::size_t i = 0; i < model.locals().size(); ++i)
            {
                const auto& local = model.locals()[i];
                if (local.name != "q" || local.methodMemberOrdinal >= members.size() ||
                    members[local.methodMemberOrdinal].name != methodName) continue;
                const std::vector<std::size_t> ordinals{i};
                const auto result = constructions.classLocalConstructionIsProvenSafe(ordinals);
                return result.size() == 1 && result[0] == expected;
            }
            return false;
        };
        if (!checkConstruction("UseSafe", true) || !checkConstruction("UseAlias", false) ||
            !checkConstruction("UseUnknown", false) || !checkConstruction("UseDestructor", false))
            return false;
        const auto bindings = constructions.constructions();
        const auto assessments = constructions.classConstructorReceiverAssessments();
        if (bindings.size() != assessments.size()) return false;
        bool publishedConstructor = false, provenInitialization = false, provenFieldCall = false,
            unprovenInitialization = false;
        for (std::size_t i = 0; i < bindings.size(); ++i)
        {
            const auto& binding = bindings[i];
            if (!binding.selectedConstructorMemberOrdinal.has_value()) continue;
            const auto ordinal = *binding.selectedConstructorMemberOrdinal;
            if (ordinal < members.size() && members[ordinal].name == "Item")
                publishedConstructor = assessments[i] == ClassConstructorReceiverAssessment::Unknown;
            if (ordinal < members.size() && members[ordinal].name == "Quiet")
                provenInitialization = assessments[i] == ClassConstructorReceiverAssessment::NoEscape;
            if (ordinal < members.size() && members[ordinal].name == "HasField")
                provenFieldCall = assessments[i] == ClassConstructorReceiverAssessment::NoEscape;
            if (ordinal < members.size() && members[ordinal].name == "HasRecursiveField")
                unprovenInitialization = assessments[i] == ClassConstructorReceiverAssessment::Unknown;
        }
        return publishedConstructor && provenInitialization && provenFieldCall && unprovenInitialization;
    }

};
}

int main()
{
    using namespace nativesharp::frontend;
    nativesharp::tests::TestContext tests;
    ClassMemberSymbol suspending;
    tests.expect(!classLocalCallableMaySuspend(suspending), "ordinary callables do not suspend");
    suspending.effects = suspending.effects.with(nativesharp::sema::Effect::Async);
    tests.expect(classLocalCallableMaySuspend(suspending), "async receiver calls are never considered safe");
    suspending.effects = nativesharp::sema::EffectSet{}.with(nativesharp::sema::Effect::Generator);
    tests.expect(classLocalCallableMaySuspend(suspending), "generator receiver calls are never considered safe");
    tests.expect(ProjectOopHirModelTestAccess::constructionIndexUsesCallableIdentity(),
        "same syntax across callables is distinct; duplicate keys fail closed and table changes invalidate cache");

    {
        std::vector<ClassReceiverEffects> effects(3, ClassReceiverEffects{false, false, false});
        effects[2].published = true;
        const std::vector<ClassReceiverEffectEdge> edges = {
            {0, 1, true, false, true, {}}, {1, 2, true, false, true, {}}};
        const auto propagated = propagateClassReceiverEffects(effects, edges);
        tests.expect(propagated[0].published && propagated[1].published,
            "receiver publication propagates transitively across resolved calls");
        const std::vector<ClassReceiverEffectEdge> open = {{0, 1, true, false, false, {}}};
        tests.expect(propagateClassReceiverEffects(effects, open)[0].unknown,
            "open dispatch cannot prove a receiver safe");
        ObjectConstructionBinding binding;
        tests.expect(assessClassConstructorReceiver(binding, effects, false) ==
                ClassConstructorReceiverAssessment::Unknown,
            "an unresolved constructor or unproved initialization remains unknown");
    }
    const auto directory = std::filesystem::temp_directory_path() / "nsc-class-value-origins";
    std::filesystem::create_directories(directory);
    const auto projectFile = directory / "Origins.nsproj";
    {
        std::ofstream project(projectFile);
        project << "<Project><PropertyGroup><Name>Origins</Name><OutputType>Exe</OutputType>"
                   "</PropertyGroup><ItemGroup><Compile Include=\"Main.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "Main.ns");
        source << R"NS(
class Item {
    static Item saved = null;
    public Item() { saved = this; }
    public Item Pick(Item other, bool cond) { return cond ? this : other; }
    public void PublishAlias() { Item alias = this; saved = alias; }
    public void Safe() { }
    public void Wrap() { this.Safe(); }
    public virtual void VirtualMethod() { }
    public void CallVirtual() { this.VirtualMethod(); }
    public void SetValue(int a) { int x = a; }
}
class Quiet { static Quiet saved; public Quiet() { } public void Safe() { } public void Unknown() { saved = this; } }
class HasField { int value = Make(); static int Make() { return 42; } public HasField() { } }
class HasRecursiveField { int value = Make(); static int Make() { return Make(); } public HasRecursiveField() { } }
class WithDestructor { static WithDestructor saved; public WithDestructor() { } ~WithDestructor() { saved = this; } public void Safe() { } }
class Program {
    static void Signal() { }
    static int Choice(Item a, Item b, bool cond) { Item value = cond ? a : b; Item copy = value; return 0; }
    static int Reassign(Item a, Item b) { Item value = a; value = b; Item copy = value; return 0; }
    static int Invalidate(Item a) { Item value = a; Signal(); Item copy = value; return 0; }
    static int UseSafe() { Quiet q = new Quiet(); q.Safe(); int unrelated = 7; return unrelated; }
    static int UseAlias() { Quiet q = new Quiet(); borrowed Quiet alias = q; return 0; }
    static int UseUnknown() { Quiet q = new Quiet(); q.Unknown(); return 0; }
    static int UseDestructor() { WithDestructor q = new WithDestructor(); q.Safe(); return 0; }
    public static int Main() { Item value = new Item(); Quiet q = new Quiet(); HasField h = new HasField(); HasRecursiveField r = new HasRecursiveField(); return 0; }
}
)NS";
    }
    ProjectCompilation project;
    std::string error;
    const bool compiled = project.compileProjectConstructions(projectFile, error);
    std::string projectDetail = error;
    for (const auto& diagnostic : project.diagnostics().diagnostics())
    {
        if (!projectDetail.empty()) projectDetail += " | ";
        projectDetail += diagnostic.message();
    }
    tests.expect(compiled, "fixture resolves names and method bodies: " + projectDetail);
    if (compiled)
    {
        tests.expect(ProjectOopHirModelTestAccess::finalizedReceiverEffects(project),
            "resolved receiver effects remain conservative for unproved publication and open dispatch");
    }
    const auto rejectedFile = directory / "Unassigned.nsproj";
    {
        std::ofstream target(rejectedFile);
        target << "<Project><PropertyGroup><Name>Unassigned</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"Unassigned.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "Unassigned.ns");
        source << "class Item { static Item saved; public int Get() { saved = this; return 1; } } "
                  "class Program { public static int Main() { Item x; return x.Get(); } }";
    }
    ProjectCompilation rejected;
    std::string rejectedError;
    tests.expect(!rejected.compileProjectMethodBodies(rejectedFile, rejectedError),
        "a class local with an unproven receiver is rejected");
    const auto localFile = directory / "LocalClass.nsproj";
    {
        std::ofstream target(localFile);
        target << "<Project><PropertyGroup><Name>LocalClass</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalClass.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalClass.ns");
        source << "class MyClass { public MyClass() {} public void SetValue(int a) { int x = a; } } "
                  "class Program { public static int Main() { MyClass myClass; int independent = 10; "
                  "myClass.SetValue(10); return independent - 10; } }";
    }
    ProjectCompilation localProject;
    std::string localError;
    tests.expect(localProject.compileProjectOopHir(localFile, localError),
        "local class construction reaches HIR with an independent statement: " + localError);
    const auto receiverIndependentFile = directory / "LocalReceiverIndependent.nsproj";
    {
        std::ofstream target(receiverIndependentFile);
        target << "<Project><PropertyGroup><Name>LocalReceiverIndependent</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalReceiverIndependent.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalReceiverIndependent.ns");
        source << "class Helper { public static int Twice(int n) { return n + n; } } "
                  "class Calculator { public Calculator() {} public int Compute(int n) { "
                  "int result = n + 2; if (result > 10) return result; return 10; } "
                  "public int Loop(int n) { int result = 0; while (n > 0) { result = result + 1; n = n - 1; } return result; } "
                  "public int StaticCall(int n) { return Helper.Twice(n); } } "
                  "class Program { public static int Main() { Calculator value; "
                  "return value.Compute(7) + value.Loop(2) + value.StaticCall(3) - 18; } }";
    }
    ProjectCompilation receiverIndependentProject;
    std::string receiverIndependentError;
    tests.expect(receiverIndependentProject.compileProjectOopHir(receiverIndependentFile, receiverIndependentError),
        "receiver-independent scalar methods with control flow reach HIR: " + receiverIndependentError);
    const auto emptyArrayFile = directory / "LocalEmptyArrayField.nsproj";
    {
        std::ofstream target(emptyArrayFile);
        target << "<Project><PropertyGroup><Name>LocalEmptyArrayField</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalEmptyArrayField.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalEmptyArrayField.ns");
        source << "class Item { private int[] values; public Item() {} "
                  "public void SetValue(int a) { int x = a; } } "
                  "class Program { public static int Main() { Item value; value.SetValue(10); return 0; } }";
    }
    ProjectCompilation emptyArrayProject;
    std::string emptyArrayError;
    tests.expect(emptyArrayProject.compileProjectOopHir(emptyArrayFile, emptyArrayError),
        "local class with an uninitialized non-owning int array field reaches HIR: " + emptyArrayError);
    const auto otherArraysFile = directory / "LocalEmptyNativeArrayFields.nsproj";
    {
        std::ofstream target(otherArraysFile);
        target << "<Project><PropertyGroup><Name>LocalEmptyNativeArrayFields</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalEmptyNativeArrayFields.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalEmptyNativeArrayFields.ns");
        source << "class Child { public int value; } "
                  "class Item { float[] numbers; Child[] children; int[,] grid; public Item() {} "
                  "public int Read() { return 7; } } "
                  "class Program { public static int Main() { Item value; return value.Read() - 7; } }";
    }
    ProjectCompilation otherArraysProject;
    std::string otherArraysError;
    tests.expect(otherArraysProject.compileProjectOopHir(otherArraysFile, otherArraysError),
        "local class with empty non-owning native arrays of several element types and ranks reaches HIR: " + otherArraysError);
    const auto emptyReferenceFile = directory / "LocalEmptyClassField.nsproj";
    {
        std::ofstream target(emptyReferenceFile);
        target << "<Project><PropertyGroup><Name>LocalEmptyClassField</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalEmptyClassField.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalEmptyClassField.ns");
        source << "class Child { public int value; } "
                  "class Item { private Child child; public Item() {} public int Read() { return 7; } } "
                  "class Program { public static int Main() { Item value; return value.Read() - 7; } }";
    }
    ProjectCompilation emptyReferenceProject;
    std::string emptyReferenceError;
    tests.expect(emptyReferenceProject.compileProjectOopHir(emptyReferenceFile, emptyReferenceError),
        "local class with an uninitialized non-owning class field reaches HIR: " + emptyReferenceError);
    const auto referenceEscapeFile = directory / "LocalClassFieldEscape.nsproj";
    {
        std::ofstream target(referenceEscapeFile);
        target << "<Project><PropertyGroup><Name>LocalClassFieldEscape</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalClassFieldEscape.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalClassFieldEscape.ns");
        source << "class Item { static Item saved; Item child; public Item() {} "
                  "public void Publish() { saved = this.child; } } "
                  "class Program { public static int Main() { Item value; value.Publish(); return 0; } }";
    }
    ProjectCompilation referenceEscapeProject;
    std::string referenceEscapeError;
    tests.expect(!referenceEscapeProject.compileProjectOopHir(referenceEscapeFile, referenceEscapeError),
        "publishing a class field from a local object remains unproven");
    const auto arrayEscapeFile = directory / "LocalArrayFieldEscape.nsproj";
    {
        std::ofstream target(arrayEscapeFile);
        target << "<Project><PropertyGroup><Name>LocalArrayFieldEscape</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalArrayFieldEscape.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalArrayFieldEscape.ns");
        source << "class Item { private int[] values; public Item() {} "
                  "public int[] Get() { return this.values; } } "
                  "class Program { public static int Main() { Item value; int[] result = value.Get(); return 0; } }";
    }
    ProjectCompilation arrayEscapeProject;
    std::string arrayEscapeError;
    tests.expect(!arrayEscapeProject.compileProjectOopHir(arrayEscapeFile, arrayEscapeError),
        "an unproven array field escape from a local class remains rejected");
    const auto initializedArrayFile = directory / "LocalInitializedArrayField.nsproj";
    {
        std::ofstream target(initializedArrayFile);
        target << "<Project><PropertyGroup><Name>LocalInitializedArrayField</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalInitializedArrayField.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalInitializedArrayField.ns");
        source << "class Item { private int[] values = new int[1]; public Item() {} "
                  "public void SetValue(int a) { int x = a; } } "
                  "class Program { public static int Main() { Item value; value.SetValue(10); return 0; } }";
    }
    ProjectCompilation initializedArrayProject;
    std::string initializedArrayError;
    tests.expect(!initializedArrayProject.compileProjectOopHir(initializedArrayFile, initializedArrayError),
        "an array field with a nontrivial initializer remains unproven");
    const auto implicitFile = directory / "ImplicitCtor.nsproj";
    {
        std::ofstream target(implicitFile);
        target << "<Project><PropertyGroup><Name>ImplicitCtor</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"ImplicitCtor.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "ImplicitCtor.ns");
        source << "class Item { public void SetValue(int a) { int x = a; } } "
                  "class Program { public static int Main() { Item item; item.SetValue(10); return 0; } }";
    }
    ProjectCompilation implicitProject;
    std::string implicitError;
    tests.expect(implicitProject.compileProjectOopHir(implicitFile, implicitError),
        "a root class with an implicit default constructor reaches HIR: " + implicitError);
    const auto destructorFile = directory / "LocalDestructor.nsproj";
    {
        std::ofstream target(destructorFile);
        target << "<Project><PropertyGroup><Name>LocalDestructor</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalDestructor.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalDestructor.ns");
        source << "class Item { public static int Drops; public Item() {} ~Item() { Drops = Drops + 1; } } "
                  "class Program { static int Early() { Item x; return 7; } "
                  "public static int Main() { Early(); if (Item.Drops != 1) return 1; "
                  "{ Item y; } if (Item.Drops != 2) return 2; return 0; } }";
    }
    ProjectCompilation destructorProject;
    std::string destructorError;
    tests.expect(destructorProject.compileProjectOopHir(destructorFile, destructorError),
        "a proven destructor with an observable scalar write reaches HIR: " + destructorError);
    const auto constructorFile = directory / "LocalConstructorObservable.nsproj";
    {
        std::ofstream target(constructorFile);
        target << "<Project><PropertyGroup><Name>LocalConstructorObservable</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalConstructorObservable.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalConstructorObservable.ns");
        source << "class Item { public static int Created; public Item() { Created = Created + 1; } } "
                  "class Program { public static int Main() { { Item value; } return Item.Created - 1; } }";
    }
    ProjectCompilation constructorProject;
    std::string constructorError;
    tests.expect(constructorProject.compileProjectOopHir(constructorFile, constructorError),
        "a proven constructor with an observable scalar write reaches HIR: " + constructorError);
    const auto scalarFile = directory / "ScalarClass.nsproj";
    {
        std::ofstream target(scalarFile);
        target << "<Project><PropertyGroup><Name>ScalarClass</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"ScalarClass.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "ScalarClass.ns");
        source << "class Counter { int value; public Counter() {} "
                  "public void Set(int a) { this.value = a; } public int Get() { return this.value; } } "
                  "class Flag { bool value; public Flag() {} "
                  "public void Set(bool a) { this.value = a; } public bool Get() { return this.value; } } "
                  "class Program { public static int Main() { Counter a; Counter b; Flag f; "
                  "a.Set(1); b.Set(2); f.Set(true); if (!f.Get()) return 1; return a.Get() + b.Get() - 3; } }";
    }
    ProjectCompilation scalarProject;
    std::string scalarError;
    tests.expect(scalarProject.compileProjectOopHir(scalarFile, scalarError),
        "two local identities and bool/int scalar field access reach HIR: " + scalarError);
    const auto scalarControlFile = directory / "LocalScalarControlFlow.nsproj";
    {
        std::ofstream target(scalarControlFile);
        target << "<Project><PropertyGroup><Name>LocalScalarControlFlow</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalScalarControlFlow.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalScalarControlFlow.ns");
        source << "class Counter { int value; public Counter() {} "
                  "public void Add(int amount) { if (amount > 0) { this.value = this.value + amount; } } "
                  "public int Get() { return this.value; } } "
                  "class Program { public static int Main() { Counter c; c.Add(3); c.Add(-1); return c.Get() - 3; } }";
    }
    ProjectCompilation scalarControlProject;
    std::string scalarControlError;
    tests.expect(scalarControlProject.compileProjectOopHir(scalarControlFile, scalarControlError),
        "local scalar field arithmetic and signed literal calls reach HIR: " + scalarControlError);
    const auto scalarLoopFile = directory / "LocalScalarLoop.nsproj";
    {
        std::ofstream target(scalarLoopFile);
        target << "<Project><PropertyGroup><Name>LocalScalarLoop</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalScalarLoop.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalScalarLoop.ns");
        source << "class Counter { int value; public Counter() {} "
                  "public void Advance(int n) { while (n > 0) { value = value + 1; n = n - 1; } } "
                  "public int Get() { return value; } } "
                  "class Program { public static int Main() { Counter c; c.Advance(3); return c.Get() - 3; } }";
    }
    ProjectCompilation scalarLoopProject;
    std::string scalarLoopError;
    tests.expect(scalarLoopProject.compileProjectOopHir(scalarLoopFile, scalarLoopError),
        "implicit scalar field operations in a loop reach HIR: " + scalarLoopError);
    const auto scalarChainFile = directory / "LocalScalarCallChain.nsproj";
    {
        std::ofstream target(scalarChainFile);
        target << "<Project><PropertyGroup><Name>LocalScalarCallChain</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalScalarCallChain.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalScalarCallChain.ns");
        source << "class Counter { int value; public Counter() {} "
                  "void Increment(int n) { this.value = this.value + n; } "
                  "public void Add(int n) { if (n > 0) { this.Increment(n); } } "
                  "public void AddImplicit(int n) { if (n > 0) { Increment(n); } } "
                  "void CycleA(int n) { if (n > 0) { this.CycleB(n - 1); } } "
                  "void CycleB(int n) { if (n > 0) { this.CycleA(n - 1); } else { this.value = this.value + 1; } } "
                  "public void Cycle(int n) { this.CycleA(n); } "
                  "public int Get() { return this.value; } } "
                  "class Program { public static int Main() { Counter c; c.Add(2); c.AddImplicit(1); c.Cycle(1); return c.Get() - 4; } }";
    }
    ProjectCompilation scalarChainProject;
    std::string scalarChainError;
    tests.expect(scalarChainProject.compileProjectOopHir(scalarChainFile, scalarChainError),
        "closed explicit and implicit receiver calls propagate scalar effects: " + scalarChainError);
    const auto scalarStaticFile = directory / "LocalScalarStaticCall.nsproj";
    {
        std::ofstream target(scalarStaticFile);
        target << "<Project><PropertyGroup><Name>LocalScalarStaticCall</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalScalarStaticCall.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalScalarStaticCall.ns");
        source << "class Counter { int value; public Counter() {} "
                  "static int Twice(int n) { return n + n; } "
                  "public void Set(int n) { this.value = n; } "
                  "public void Update() { this.value = Twice(this.value); } "
                  "public int Get() { return this.value; } } "
                  "class Program { public static int Main() { Counter c; c.Set(3); c.Update(); return c.Get() - 6; } }";
    }
    ProjectCompilation scalarStaticProject;
    std::string scalarStaticError;
    tests.expect(scalarStaticProject.compileProjectOopHir(scalarStaticFile, scalarStaticError),
        "static scalar value call on a local receiver field reaches HIR: " + scalarStaticError);
    const auto negativeArgumentFile = directory / "LocalSignedArgument.nsproj";
    {
        std::ofstream target(negativeArgumentFile);
        target << "<Project><PropertyGroup><Name>LocalSignedArgument</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalSignedArgument.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalSignedArgument.ns");
        source << "class Counter { int value; public Counter() {} public void Set(int n) { this.value = n; } "
                  "public int Get() { return this.value; } } "
                  "class Program { public static int Main() { Counter c; c.Set(-1); return c.Get() + 1; } }";
    }
    ProjectCompilation negativeArgumentProject;
    std::string negativeArgumentError;
    tests.expect(negativeArgumentProject.compileProjectOopHir(negativeArgumentFile, negativeArgumentError),
        "a builtin signed integer argument to a local class method reaches HIR: " + negativeArgumentError);
    const auto floatingFile = directory / "LocalFloatingFields.nsproj";
    {
        std::ofstream target(floatingFile);
        target << "<Project><PropertyGroup><Name>LocalFloatingFields</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalFloatingFields.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalFloatingFields.ns");
        source << "class Sensor { float single; double wide; public Sensor() {} "
                  "public void SetSingle(float x) { this.single = x; } public float GetSingle() { return this.single; } "
                  "public void SetWide(double x) { this.wide = x; } public double GetWide() { return this.wide; } } "
                  "class Program { public static int Main() { Sensor s; "
                  "if (s.GetSingle() != 0.0f || s.GetWide() != 0.0) return 1; "
                  "s.SetSingle(1.5f); s.SetWide(2.25); "
                  "if (s.GetSingle() != 1.5f || s.GetWide() != 2.25) return 2; return 0; } }";
    }
    ProjectCompilation floatingProject;
    std::string floatingError;
    tests.expect(floatingProject.compileProjectOopHir(floatingFile, floatingError),
        "local class float and double fields preserve zero initialization and scalar access in HIR: " + floatingError);
    const auto floatingInitializedFile = directory / "LocalFloatingInitialized.nsproj";
    {
        std::ofstream target(floatingInitializedFile);
        target << "<Project><PropertyGroup><Name>LocalFloatingInitialized</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalFloatingInitialized.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalFloatingInitialized.ns");
        source << "class Sensor { float single = 1.5f; double wide = 2.25; public Sensor() {} "
                  "public float GetSingle() { return this.single; } public double GetWide() { return this.wide; } } "
                  "class Program { public static int Main() { Sensor s; "
                  "if (s.GetSingle() != 1.5f || s.GetWide() != 2.25) return 1; return 0; } }";
    }
    ProjectCompilation floatingInitializedProject;
    std::string floatingInitializedError;
    tests.expect(floatingInitializedProject.compileProjectOopHir(floatingInitializedFile, floatingInitializedError),
        "local class same-type float and double field initializers reach HIR: " + floatingInitializedError);
    const auto floatingCallFile = directory / "LocalFloatingFieldCall.nsproj";
    {
        std::ofstream target(floatingCallFile);
        target << "<Project><PropertyGroup><Name>LocalFloatingFieldCall</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalFloatingFieldCall.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalFloatingFieldCall.ns");
        source << "class Sensor { float single = Make(); static float Make() { return 1.5f; } "
                  "public Sensor() {} public float Get() { return this.single; } } "
                  "class Program { public static int Main() { Sensor s; if (s.Get() != 1.5f) return 1; return 0; } }";
    }
    ProjectCompilation floatingCallProject;
    std::string floatingCallError;
    tests.expect(floatingCallProject.compileProjectOopHir(floatingCallFile, floatingCallError),
        "a resolved trivial float initializer call reaches HIR: " + floatingCallError);
    const auto initializerFile = directory / "LocalInitializers.nsproj";
    {
        std::ofstream target(initializerFile);
        target << "<Project><PropertyGroup><Name>LocalInitializers</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalInitializers.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalInitializers.ns");
        source << "class Item { int value = 40; bool flag = true; int stamp = 7; "
                  "public Item() { this.value = 42; this.flag = false; } "
                  "public int Get() { return this.value; } public bool Flag() { return this.flag; } "
                  "public int Stamp() { return this.stamp; } } "
                  "class Program { public static int Main() { Item value; if (value.Get() != 42) return 1; "
                  "if (value.Flag()) return 2; return value.Stamp() - 7; } }";
    }
    ProjectCompilation initializerProject;
    std::string initializerError;
    tests.expect(initializerProject.compileProjectOopHir(initializerFile, initializerError),
        "literal field initializers and bound scalar constructor writes reach HIR: " + initializerError);
    const auto defaultsFile = directory / "LocalScalarDefaults.nsproj";
    {
        std::ofstream target(defaultsFile);
        target << "<Project><PropertyGroup><Name>LocalScalarDefaults</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalScalarDefaults.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalScalarDefaults.ns");
        source << "class Item { int value = 2 + 3; int stamp = 20 + 22; bool flag = !false; "
                  "public Item(int x = 7, bool b = true) { this.value = x; this.flag = b; } "
                  "public int Get() { return this.value; } public int Stamp() { return this.stamp; } "
                  "public bool Flag() { return this.flag; } } "
                  "class Program { public static int Main() { Item item; if (!item.Flag()) return 1; "
                  "return item.Get() + item.Stamp() - 49; } }";
    }
    ProjectCompilation defaultsProject;
    std::string defaultsError;
    tests.expect(defaultsProject.compileProjectOopHir(defaultsFile, defaultsError),
        "literal-only scalar expressions and resolved scalar defaults reach HIR: " + defaultsError);
    const auto fieldCallFile = directory / "LocalFieldCall.nsproj";
    {
        std::ofstream target(fieldCallFile);
        target << "<Project><PropertyGroup><Name>LocalFieldCall</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalFieldCall.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalFieldCall.ns");
        source << "class L { int field = Make(); static int Make() { return 1; } "
                  "public L() {} public int Get() { return this.field; } } "
                  "class P { public static int Main() { L value; return value.Get() - 1; } }";
    }
    ProjectCompilation fieldCallProject;
    std::string fieldCallError;
    tests.expect(fieldCallProject.compileProjectOopHir(fieldCallFile, fieldCallError),
        "resolved zero-argument source initializer returning a literal reaches HIR: " + fieldCallError);
    const auto inheritanceFile = directory / "LocalSourceInheritance.nsproj";
    {
        std::ofstream target(inheritanceFile);
        target << "<Project><PropertyGroup><Name>LocalSourceInheritance</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalSourceInheritance.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalSourceInheritance.ns");
        source << "class Base { public static int Drops; int a = 1; public Base() { this.a = 2; } "
                  "~Base() { Drops = Drops + 1; } public int ReadBase() { return this.a; } } "
                  "class Derived : Base { int b = 3; public Derived() { this.b = 4; } "
                  "public int ReadDerived() { return this.b; } } "
                  "class Program { static int Use() { Derived d; return d.ReadBase() + d.ReadDerived() - 6; } "
                  "public static int Main() { if (Use() != 0) return 1; return Base.Drops - 1; } }";
    }
    ProjectCompilation inheritanceProject;
    std::string inheritanceError;
    tests.expect(inheritanceProject.compileProjectOopHir(inheritanceFile, inheritanceError),
        "source-only base with proven constructors and destruction reaches HIR: " + inheritanceError);
    const auto deepFile = directory / "LocalDeepInheritance.nsproj";
    {
        std::ofstream target(deepFile);
        target << "<Project><PropertyGroup><Name>LocalDeepInheritance</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalDeepInheritance.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalDeepInheritance.ns");
        source << "class A { int a = 1; public A() {} public int First() { return this.a; } } "
                  "class B : A { int b = 2; public B() {} public int Second() { return this.b; } } "
                  "class C : B { int c = 3; public C() {} public int Third() { return this.c; } } "
                  "class Program { public static int Main() { C a; C b; "
                  "return a.First() + b.Third() - 4; } }";
    }
    ProjectCompilation deepProject;
    std::string deepError;
    tests.expect(deepProject.compileProjectOopHir(deepFile, deepError),
        "three source classes and two independent local objects reach HIR: " + deepError);
    const auto virtualFile = directory / "LocalVirtual.nsproj";
    {
        std::ofstream target(virtualFile);
        target << "<Project><PropertyGroup><Name>LocalVirtual</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalVirtual.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalVirtual.ns");
        source << "class Base { int a = 1; public Base() {} public virtual int Get() { return this.a; } } "
                  "class Derived : Base { int b = 2; public Derived() {} "
                  "public override int Get() { return this.b; } } "
                  "class Program { public static int Main() { Derived d; return d.Get() - 2; } }";
    }
    ProjectCompilation virtualProject;
    std::string virtualError;
    tests.expect(virtualProject.compileProjectOopHir(virtualFile, virtualError),
        "closed source override on an exact local type reaches HIR: " + virtualError);
    const auto exceptionFile = directory / "LocalExceptionCleanup.nsproj";
    {
        std::ofstream target(exceptionFile);
        target << "<Project><PropertyGroup><Name>LocalExceptionCleanup</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalExceptionCleanup.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalExceptionCleanup.ns");
        source << "namespace NativeSharp.Core { public class Exception {} } "
                  "class Item { public static int Drops; public Item() {} ~Item() { Drops = Drops + 1; } } "
                  "class Program { static void Fail() throws NativeSharp.Core.Exception { Item item; "
                  "throw new NativeSharp.Core.Exception(); } "
                  "public static int Main() { try { Fail(); } catch (NativeSharp.Core.Exception e) {} "
                  "return Item.Drops - 1; } }";
    }
    ProjectCompilation exceptionProject;
    std::string exceptionError;
    tests.expect(exceptionProject.compileProjectOopHir(exceptionFile, exceptionError),
        "explicit throw of a new exception retains local-object cleanup: " + exceptionError);
    const auto finallyFile = directory / "LocalFinallyCleanup.nsproj";
    {
        std::ofstream target(finallyFile);
        target << "<Project><PropertyGroup><Name>LocalFinallyCleanup</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalFinallyCleanup.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalFinallyCleanup.ns");
        source << "class Item { public static int Drops; public static int Observed; "
                  "public Item() {} ~Item() { Drops = Drops + 1; } } "
                  "class Program { static int Inner() { try { Item item; return 7; } "
                  "finally { Item.Observed = Item.Drops; } } "
                  "static int Outer() { Item item; try { return 7; } "
                  "finally { Item.Observed = Item.Drops; } } "
                  "public static int Main() { return 0; } }";
    }
    ProjectCompilation finallyProject;
    std::string finallyError;
    tests.expect(finallyProject.compileProjectOopHir(finallyFile, finallyError),
        "bounded try/finally with inner and outer class locals reaches HIR: " + finallyError);
    const auto catchReturnFile = directory / "LocalCatchReturn.nsproj";
    {
        std::ofstream target(catchReturnFile);
        target << "<Project><PropertyGroup><Name>LocalCatchReturn</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalCatchReturn.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalCatchReturn.ns");
        source << "namespace NativeSharp.Core { public class Exception {} } "
                  "class Item { public static int Drops; public static int Seen; public Item() {} "
                  "~Item() { Drops = Drops + 1; } } class Program { "
                  "static int Use() { try { Item item; return 7; } "
                  "catch (NativeSharp.Core.Exception e) { return 8; } finally { Item.Seen = Item.Drops; } } "
                  "public static int Main() { return 0; } }";
    }
    ProjectCompilation catchReturnProject;
    std::string catchReturnError;
    tests.expect(catchReturnProject.compileProjectOopHir(catchReturnFile, catchReturnError),
        "bounded try/catch/finally return with class local reaches HIR: " + catchReturnError);
    const auto caughtThrowFile = directory / "LocalCaughtThrow.nsproj";
    {
        std::ofstream target(caughtThrowFile);
        target << "<Project><PropertyGroup><Name>LocalCaughtThrow</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalCaughtThrow.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalCaughtThrow.ns");
        source << "namespace NativeSharp.Core { public class Exception {} } "
                  "class Item { public static int Drops; public static int Seen; public Item() {} "
                  "~Item() { Drops = Drops + 1; } } class Program { "
                  "static void Use() { try { Item item; throw new NativeSharp.Core.Exception(); } "
                  "catch (NativeSharp.Core.Exception e) { Item.Seen = Item.Drops; } finally {} } "
                  "public static int Main() { return 0; } }";
    }
    ProjectCompilation caughtThrowProject;
    std::string caughtThrowError;
    tests.expect(caughtThrowProject.compileProjectOopHir(caughtThrowFile, caughtThrowError),
        "bounded caught throw with source exception and class local reaches HIR: " + caughtThrowError);
    const auto directFile = directory / "ImplicitField.nsproj";
    {
        std::ofstream target(directFile);
        target << "<Project><PropertyGroup><Name>ImplicitField</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"ImplicitField.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "ImplicitField.ns");
        source << "class Node { public int value = 42; } class Program { "
                  "static int Read() { Node n; return n.value; } "
                  "static int Assign() { Node n; n.value = 7; return n.value; } "
                  "public static int Main() { return Read() + Assign() - 49; } }";
    }
    ProjectCompilation directProject;
    std::string directError;
    tests.expect(directProject.compileProjectOopHir(directFile, directError),
        "implicit construction and direct scalar read/write reach HIR: " + directError);
    const auto siblingsFile = directory / "SiblingClassLocals.nsproj";
    {
        std::ofstream target(siblingsFile);
        target << "<Project><PropertyGroup><Name>SiblingClassLocals</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"SiblingClassLocals.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "SiblingClassLocals.ns");
        source << "class Item { public Item() {} public void Safe() {} } "
                  "class Program { public static int Main() { { Item same; same.Safe(); } "
                  "{ Item same; same.Safe(); } { int same = 3; int copy = same; } return 0; } }";
    }
    ProjectCompilation siblingsProject;
    std::string siblingsError;
    tests.expect(siblingsProject.compileProjectOopHir(siblingsFile, siblingsError),
        "sibling locals with identical names use their resolved ordinals: " + siblingsError);
    const auto scalarUsesFile = directory / "ScalarLocalUses.nsproj";
    {
        std::ofstream target(scalarUsesFile);
        target << "<Project><PropertyGroup><Name>ScalarLocalUses</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"ScalarLocalUses.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "ScalarLocalUses.ns");
        source << "class Item { public int field; public Item() {} "
                  "public void SetValue(int a) { this.field = a; } } "
                  "class Program { static int Use(int input) { Item item; item.SetValue(input); "
                  "int copy = item.field; item.SetValue(copy); return item.field; } "
                  "public static int Main() { return Use(7) - 7; } }";
    }
    ProjectCompilation scalarUsesProject;
    std::string scalarUsesError;
    tests.expect(scalarUsesProject.compileProjectOopHir(scalarUsesFile, scalarUsesError),
        "by-value scalar arguments and scalar field copies reach HIR: " + scalarUsesError);
    const auto borrowedFrameFile = directory / "BorrowedFrameLocal.nsproj";
    {
        std::ofstream target(borrowedFrameFile);
        target << "<Project><PropertyGroup><Name>BorrowedFrameLocal</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"BorrowedFrameLocal.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "BorrowedFrameLocal.ns");
        source << "class Item { public int value = 7; public Item() {} "
                  "public int Read() { return this.value; } } "
                  "class Program { public static int Main() { Item item; "
                  "borrowed Item alias = item; return alias.Read() - 7; } }";
    }
    ProjectCompilation borrowedFrameProject;
    std::string borrowedFrameError;
    tests.expect(borrowedFrameProject.compileProjectOopHir(borrowedFrameFile, borrowedFrameError),
        "same-scope borrowed alias of a frame class reaches HIR: " + borrowedFrameError);
    const auto nestedBorrowFile = directory / "BorrowedFrameNested.nsproj";
    {
        std::ofstream target(nestedBorrowFile);
        target << "<Project><PropertyGroup><Name>BorrowedFrameNested</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"BorrowedFrameNested.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "BorrowedFrameNested.ns");
        source << "class Item { public int value = 7; public Item() {} "
                  "public int Read() { return this.value; } } "
                  "class Program { public static int Main() { Item owner; "
                  "{ borrowed Item alias = owner; alias.Read(); } { int alias = 3; } "
                  "return owner.Read() - 7; } }";
    }
    ProjectCompilation nestedBorrowProject;
    std::string nestedBorrowError;
    tests.expect(nestedBorrowProject.compileProjectOopHir(nestedBorrowFile, nestedBorrowError),
        "nested frame borrow with sibling shadow reaches HIR: " + nestedBorrowError);
    const auto siblingBorrowFile = directory / "BorrowedFrameSibling.nsproj";
    {
        std::ofstream target(siblingBorrowFile);
        target << "<Project><PropertyGroup><Name>BorrowedFrameSibling</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"BorrowedFrameSibling.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "BorrowedFrameSibling.ns");
        source << "class Item { public Item() {} public int Read() { return 7; } } "
                  "class Program { public static int Main() { Item owner; "
                  "{ borrowed Item alias = owner; } { return alias.Read(); } } }";
    }
    ProjectCompilation siblingBorrowProject;
    std::string siblingBorrowError;
    tests.expect(!siblingBorrowProject.compileProjectOopHir(siblingBorrowFile, siblingBorrowError),
        "a borrowed alias is not visible in a sibling scope");
    const auto aliasFile = directory / "LocalAlias.nsproj";
    {
        std::ofstream target(aliasFile);
        target << "<Project><PropertyGroup><Name>LocalAlias</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"LocalAlias.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "LocalAlias.ns");
        source << "class MyClass { public MyClass() {} } "
                  "class Program { public static int Main() { MyClass local; MyClass alias = local; return 0; } }";
    }
    ProjectCompilation aliasProject;
    std::string aliasError;
    tests.expect(!aliasProject.compileProjectOopHir(aliasFile, aliasError),
        "a local class alias remains rejected before HIR");
    const auto scopeStructFile = directory / "ScopeStruct.nsproj";
    {
        std::ofstream target(scopeStructFile);
        target << "<Project><PropertyGroup><Name>ScopeStruct</Name><OutputType>Exe</OutputType>"
                  "</PropertyGroup><ItemGroup><Compile Include=\"ScopeStruct.ns\" /></ItemGroup></Project>";
        std::ofstream source(directory / "ScopeStruct.ns");
        source << "struct Value { public int x; } "
                  "class Program { public static int Main() { scope Value value; return 0; } }";
    }
    ProjectCompilation scopeStructProject;
    std::string scopeStructError;
    tests.expect(scopeStructProject.compileProjectOopHir(scopeStructFile, scopeStructError),
        "a scope struct is not treated as an implicit class construction: " + scopeStructError);
    struct NegativeCase { const char* name; const char* source; const char* expected; };
    const NegativeCase negativeCases[] = {
        {"LocalClassFieldSelfEscape", "class Item { Item child; public Item() { child = this; } "
            "public int Read() { return 7; } } "
            "class P { public static int Main() { Item value; return value.Read() - 7; } }",
            "local class construction or destruction is not proven safe"},
        {"LocalClassFieldAliasEscape", "class Item { public Item child; public Item() {} } "
            "class P { public static int Main() { Item value; value.child = value; return 0; } }",
            "cannot be stored in a field or static"},
        {"LocalOwnedClassField", "class Child { public int value; } "
            "class Item { owned Child child; public Item() {} public int Read() { return 7; } } "
            "class P { public static int Main() { Item value; return value.Read() - 7; } }",
            "use of unassigned local variable"},
        {"LocalOwnedArrayField", "class Child { public int value; } "
            "class Item { owned Child[] children; public Item() {} public int Read() { return 7; } } "
            "class P { public static int Main() { Item value; return value.Read() - 7; } }",
            "use of unassigned local variable"},
        {"LocalReferenceArrayFieldEscape", "class Child { public int value; } "
            "class Item { static Child[] saved; Child[] children; public Item() {} "
            "public void Publish() { saved = this.children; } } "
            "class P { public static int Main() { Item value; value.Publish(); return 0; } }",
            "local class object has an unproven use"},
        {"LocalThis", "class L { public static L saved; public void Leak() { saved = this; } } "
            "class P { public static int Main() { L value; value.Leak(); return 0; } }",
            "local class object has an unproven use"},
        {"LocalConditional", "class L {} class P { static L Leak(bool b) { L a; L c; return b ? a : c; } "
            "public static int Main() { return 0; } }", "local class object has an unproven use"},
        {"LocalConstructorThis", "class L { public static L saved; public L() { saved = this; } } "
            "class P { public static int Main() { L value; return 0; } }",
            "local class construction or destruction is not proven safe"},
        {"LocalConstructorStaticInitializer", "class Marker { public static int value; } "
            "class State { public static int value; static State() { Marker.value = 42; } } "
            "class L { public L() { State.value = 1; } } "
            "class P { public static int Main() { L value; return 0; } }",
            "local class construction or destruction is not proven safe"},
        {"LocalDestructorStaticInitializer", "class State { public static int value = Make(); "
            "static int Make() { return 1; } } "
            "class L { public L() {} ~L() { State.value = 1; } } "
            "class P { public static int Main() { L value; return 0; } }",
            "local class construction or destruction is not proven safe"},
        {"LocalConstructorMixed", "class L { int field = 1; static L saved; "
            "public L() { this.field = 3; saved = this; } } "
            "class P { public static int Main() { L value; return 0; } }",
            "local class construction or destruction is not proven safe"},
        {"LocalBaseConstructorEscape", "class B { static B saved; public B() { saved = this; } } "
            "class D : B { public D() {} } class P { public static int Main() { D value; return 0; } }",
            "local class construction or destruction is not proven safe"},
        {"LocalBaseDestructorEscape", "class B { static B saved; public B() {} ~B() { saved = this; } } "
            "class D : B { public D() {} } class P { public static int Main() { D value; return 0; } }",
            "local class construction or destruction is not proven safe"},
        {"LocalMiddleConstructorEscape", "class A { public A() {} } "
            "class B : A { static B saved; public B() { saved = this; } } "
            "class C : B { public C() {} } class P { public static int Main() { C value; return 0; } }",
            "local class construction or destruction is not proven safe"},
        {"LocalRootDestructorEscape", "class A { static A saved; public A() {} ~A() { saved = this; } } "
            "class B : A { public B() {} } class C : B { public C() {} } "
            "class P { public static int Main() { C value; return 0; } }",
            "local class construction or destruction is not proven safe"},
        {"LocalBaseVirtual", "class B { public B() {} public virtual void Touch() {} } "
            "class D : B { static D saved; public D() {} public override void Touch() { saved = this; } } "
            "class P { public static int Main() { D value; value.Touch(); return 0; } }",
            "local class construction or destruction is not proven safe"},
        {"LocalInitializerRecursive", "class L { int field = Make(); static int Make() { return Make(); } "
            "public L() {} public int Get() { return this.field; } } "
            "class P { public static int Main() { L value; return value.Get(); } }",
            "local class construction or destruction is not proven safe"},
        {"LocalInitializerStaticEffect", "class L { static int seed = Init(); static int Init() { return 2; } "
            "int field = Make(); static int Make() { return 1; } public L() {} "
            "public int Get() { return this.field; } } class P { public static int Main() { "
            "L value; return value.Get(); } }", "local class construction or destruction is not proven safe"},
        {"LocalInitializerConversion", "class L { long field = Make(); static int Make() { return 1; } "
            "public L() {} public long Get() { return this.field; } } class P { public static int Main() { "
            "L value; value.Get(); return 0; } }", "local class construction or destruction is not proven safe"},
        {"LocalLambda", "delegate int Read(); class L { public L() {} } "
            "class P { public static int Main() { L value; raii Read f = [copy value] () => 0; return 0; } }",
            "local class object has an unproven use"},
        {"LocalBorrowReturn", "class L { public L() {} } "
            "class P { static borrowed L Leak() { L value; borrowed L alias = value; return alias; } "
            "public static int Main() { return 0; } }", "local class object has an unproven use"},
        {"LocalBorrowCapture", "delegate int Read(); class L { public L() {} } "
            "class P { public static int Main() { L value; borrowed L alias = value; "
            "raii Read f = [copy alias] () => 0; return 0; } }", "local class object has an unproven use"},
        {"LocalBorrowInnerOwner", "class L { public L() {} } class P { public static int Main() { "
            "L outer; borrowed L alias = outer; { L inner; alias = inner; } return 0; } }",
            "local class object has an unproven use"},
        {"LocalFinally", "class L { public L() {} public void Safe() {} } "
            "class P { public static int Main() { L value; try { value.Safe(); } "
            "finally { int x = 0; } return 0; } }", "local class object has an unproven use"},
        {"LocalFinallyStaticInitializer", "class State { public static int value; "
            "static State() { value = 5; } } class L { public L() {} } "
            "class P { public static int Main() { try { L value; } "
            "finally { State.value = 1; } return 0; } }", "local class object has an unproven use"},
        {"LocalFinallyCast", "class L { public L() {} } class P { public static int Main() { "
            "try { L value; int copy = (int) 3; } finally {} return 0; } }",
            "local class object has an unproven use"},
        {"LocalCatch", "namespace NativeSharp.Core { public class Exception {} } "
            "class L { public L() {} } class P { public static int Main() { "
            "try { L value; } catch (NativeSharp.Core.Exception e) {} return 0; } }",
            "local class object has an unproven use"},
        {"LocalCatchExceptionArgument", "namespace NativeSharp.Core { public class Exception { "
            "public Exception(int x) {} } } class L { public L() {} } class P { "
            "static void Use() { try { L value; throw new NativeSharp.Core.Exception(1); } "
            "catch (NativeSharp.Core.Exception e) {} finally {} } public static int Main() { return 0; } }",
            "local class object has an unproven use"},
        {"LocalAsync", "class L { int field; public L() {} public async int Get() { return this.field; } } "
            "class P { public static int Main() { L value; value.Get(); return 0; } }",
            "local class object has an unproven use"},
        {"LocalExceptionalUnknownCall", "namespace NativeSharp.Core { public class Exception {} } "
            "class L { public L() {} } class P { static int Call() { return 1; } "
            "static int Fail() throws NativeSharp.Core.Exception { L value; return Call(); } "
            "public static int Main() { return 0; } }", "local class object has an unproven use"},
        {"LocalDestructorThis", "class L { static L saved; public L() {} ~L() { saved = this; } } "
            "class P { public static int Main() { L value; return 0; } }",
            "local class construction or destruction is not proven safe"},
        {"LocalDestructorCall", "class L { static L saved; public L() {} "
            "void Leak() { saved = this; } ~L() { this.Leak(); } } "
            "class P { public static int Main() { L value; return 0; } }",
            "local class construction or destruction is not proven safe"},
        {"LocalFieldAddress", "class L { public int field; } "
            "class P { static int* Leak() { L value; return &value.field; } "
            "public static int Main() { return 0; } }", "local class object has an unproven use"},
        {"LocalFloatingFieldAddress", "class L { public float field; } "
            "class P { static float* Leak() { L value; return &value.field; } "
            "public static int Main() { return 0; } }", "local class object has an unproven use"},
        {"LocalImplicitReceiverLeak", "class L { static L saved; public L() {} "
            "int Leak(int n) { saved = this; return n; } "
            "public int Compute(int n) { int x = Leak(n); return x; } } "
            "class P { public static int Main() { L value; return value.Compute(1); } }",
            "local class object has an unproven use"},
        {"LocalScalarControlLeak", "class L { static L saved; int field; public L() {} "
            "public void Add(int n) { if (n > 0) { this.field = this.field + n; } saved = this; } } "
            "class P { public static int Main() { L value; value.Add(1); return 0; } }",
            "local class object has an unproven use"},
        {"LocalScalarCallLeak", "class L { static L saved; int field; public L() {} "
            "static void Capture(L value) { saved = value; } "
            "public void Add(int n) { this.field = n; Capture(this); } } "
            "class P { public static int Main() { L value; value.Add(1); return 0; } }",
            "local class object has an unproven use"},
        {"LocalScalarNestedLeak", "class L { static L saved; int field; public L() {} "
            "void Leak(int n) { saved = this; } "
            "public void Add(int n) { if (n > 0) { this.Leak(n); } } } "
            "class P { public static int Main() { L value; value.Add(1); return 0; } }",
            "local class object has an unproven use"},
        {"LocalScalarStaticRef", "class L { int field; public L() {} "
            "static void Mutate(ref int value) { value = 1; } "
            "public void Work() { Mutate(ref this.field); } } "
            "class P { public static int Main() { L value; value.Work(); return 0; } }",
            "local class object has an unproven use"},
        {"LocalScalarTransitiveLeak", "class L { static L saved; int field; public L() {} "
            "void Leak(int n) { saved = this; } "
            "void Mid(int n) { if (n > 0) { this.Leak(n); } } "
            "public void Add(int n) { if (n > 0) { this.Mid(n); } } } "
            "class P { public static int Main() { L value; value.Add(1); return 0; } }",
            "local class object has an unproven use"},
        {"LocalScalarCyclicLeak", "class L { static L saved; public L() {} "
            "void Leak(int n) { saved = this; } "
            "void A(int n) { if (n > 0) { this.B(n - 1); } } "
            "void B(int n) { if (n > 0) { this.A(n - 1); } else { this.Leak(n); } } "
            "public void Start(int n) { this.A(n); } } "
            "class P { public static int Main() { L value; value.Start(1); return 0; } }",
            "local class object has an unproven use"},
        {"LocalScalarConversionLeak", "class L { static L saved; public L() {} "
            "public static implicit operator int(L value) { saved = value; return 0; } "
            "int ConvertReceiver() { return this; } "
            "public void Work(int n) { if (n > 0) { this.ConvertReceiver(); } } } "
            "class P { public static int Main() { L value; value.Work(1); return 0; } }",
            "local class object has an unproven use"},
        {"LocalScalarVirtualLeak", "class L { static L saved; int field; public L() {} "
            "public virtual void Leak(int n) { saved = this; } "
            "public void Add(int n) { if (n > 0) { this.Leak(n); } } } "
            "class P { public static int Main() { L value; value.Add(1); return 0; } }",
            "local class object has an unproven use"},
        {"LocalFieldRef", "class L { public int field; } "
            "class P { static ref int Leak() { L value; return ref value.field; } "
            "public static int Main() { return 0; } }", "local class object has an unproven use"},
        {"LocalSiblingEscape", "class L { public L() {} public void Safe() {} } "
            "class P { static L saved; public static int Main() { "
            "{ L same; same.Safe(); } { L same; saved = same; } return 0; } }",
            "local class object has an unproven use"},
        {"InvalidRaiiOwnerCopy", "class L { public L() {} } "
            "class P { public static int Main() { L owner = new L(); L alias = owner; return 0; } }",
            "cannot be copied into a non-owning local"},
    };
    for (const NegativeCase& item : negativeCases)
    {
        const std::string name(item.name);
        const auto file = directory / (name + ".nsproj");
        {
            std::ofstream target(file);
            target << "<Project><PropertyGroup><Name>" << name << "</Name><OutputType>Exe</OutputType>"
                      "</PropertyGroup><ItemGroup><Compile Include=\"" << name << ".ns\" /></ItemGroup></Project>";
            std::ofstream source(directory / (name + ".ns"));
            source << item.source;
        }
        ProjectCompilation negative;
        std::string negativeError;
        const bool built = negative.compileProjectOopHir(file, negativeError);
        bool expectedDiagnostic = false;
        for (const auto& diagnostic : negative.diagnostics().diagnostics())
            if (diagnostic.message().find(item.expected) != std::string::npos) expectedDiagnostic = true;
        tests.expect(!built && expectedDiagnostic, name + " is rejected by the class-local safety gate");
    }
    return tests.result();
}
