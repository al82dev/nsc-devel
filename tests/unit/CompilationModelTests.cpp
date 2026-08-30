#include "nativesharp/Compilation/PartialTypeAggregator.h"
#include "nativesharp/Compilation/ProjectSourceSet.h"
#include "nativesharp/Compilation/TypeIdentity.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
using nativesharp::compilation::DeclaredAccessibility;
using nativesharp::compilation::PartialTypeAggregator;
using nativesharp::compilation::ProjectSourceSet;
using nativesharp::compilation::TypeDeclarationKind;
using nativesharp::compilation::TypeIdentity;
using nativesharp::compilation::TypeNameSegment;
using nativesharp::compilation::TypePartDescriptor;
using nativesharp::tests::TestContext;

TypeIdentity playerIdentity()
{
    return TypeIdentity("Game", {"Game", "Entities"}, {}, "Player");
}

TypePartDescriptor playerPart(std::size_t ordinal, bool isPartial = true)
{
    return TypePartDescriptor{
        playerIdentity(),
        TypeDeclarationKind::Class,
        DeclaredAccessibility::Unspecified,
        isPartial,
        false,
        false,
        false,
        std::nullopt,
        ordinal,
        10,
        "Player" + std::to_string(ordinal) + ".ns"};
}

void testProjectSources(TestContext& tests)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage21_project_sources";
    std::error_code errorCode;
    std::filesystem::remove_all(root, errorCode);
    std::filesystem::create_directories(root / "Models", errorCode);

    {
        std::ofstream(root / "Main.ns") << "int main() { return 0; }\n";
        std::ofstream(root / "Models" / "Player.ns") << "// stage 21 fixture\n";
        std::ofstream(root / "notes.txt") << "not source\n";
    }

    ProjectSourceSet sources(root);
    std::string error;
    tests.expect(sources.add("Models/Player.ns", error), "project accepts a nested .ns source");
    tests.expect(sources.add("Main.ns", error), "project accepts a root .ns source");
    tests.expect(sources.finalize(error), "project source set finalizes");
    tests.expect(sources.isFinalized(), "project source set records finalization");
    tests.expect(sources.sources().size() == 2, "project contains both source files");
    tests.expect(
        sources.sources().size() == 2 && sources.sources()[0].projectRelativePath.generic_string() == "Main.ns",
        "source order is canonical rather than insertion order");
    tests.expect(
        sources.sources().size() == 2 && sources.sources()[1].ordinal == 1,
        "canonical source order assigns stable ordinals");

    std::string postFinalizeError;
    tests.expect(!sources.add("Main.ns", postFinalizeError), "project rejects additions after finalization");

    ProjectSourceSet duplicateSources(root);
    tests.expect(duplicateSources.add("Main.ns", error), "duplicate fixture first insertion succeeds");
    tests.expect(!duplicateSources.add("./Main.ns", error), "duplicate logical source is rejected");

    ProjectSourceSet wrongExtension(root);
    tests.expect(!wrongExtension.add("notes.txt", error), "non-.ns source is rejected");

    ProjectSourceSet empty(root);
    tests.expect(!empty.finalize(error), "empty project is rejected");

    const std::filesystem::path outside = root.parent_path() / "nativesharp_stage21_outside.ns";
    std::ofstream(outside) << "// outside\n";
    ProjectSourceSet escaping(root);
    tests.expect(!escaping.add(outside, error), "source outside project root is rejected");

    std::filesystem::remove(outside, errorCode);
    std::filesystem::remove_all(root, errorCode);
}

void testTypeIdentity(TestContext& tests)
{
    const TypeIdentity player = playerIdentity();
    tests.expect(player.isValid(), "module-qualified class identity is valid");
    tests.expect(player.fullyQualifiedName() == "Game.Entities.Player", "fully qualified type name is stable");
    tests.expect(player.key() == "Game::Game.Entities.Player", "module participates in type identity");

    const TypeIdentity movedPlayer("Game", {"Game", "Entities"}, {}, "Player");
    tests.expect(player == movedPlayer, "source path does not participate in type identity");

    const TypeIdentity otherModule("Editor", {"Game", "Entities"}, {}, "Player");
    tests.expect(!(player == otherModule), "equal namespace and name in another module is a different type");

    const TypeIdentity nested("Game", {"Game"}, {TypeNameSegment{"World", 1}}, "Entity", 2);
    tests.expect(nested.fullyQualifiedName() == "Game.World`1+Entity`2", "nested generic arity is encoded");
    const TypeIdentity namespaceType("Game", {"Game", "World"}, {}, "Entity", 2);
    tests.expect(nested.key() != namespaceType.key(), "nested type cannot collide with a namespace type");

    const TypeIdentity invalidModule("bad-module", {}, {}, "Player");
    tests.expect(!invalidModule.isValid(), "invalid module identifier is rejected");
    const TypeIdentity invalidNamespace("Game", {"bad.namespace"}, {}, "Player");
    tests.expect(!invalidNamespace.isValid(), "invalid namespace segment is rejected");
    const TypeIdentity invalidType("Game", {}, {}, "9Player");
    tests.expect(!invalidType.isValid(), "invalid type identifier is rejected");
}

void testPartialAggregationPositive(TestContext& tests)
{
    PartialTypeAggregator aggregator;
    std::string error;
    TypePartDescriptor actions = playerPart(1);
    actions.accessibility = DeclaredAccessibility::Public;
    actions.isSealed = true;
    TypePartDescriptor state = playerPart(0);
    state.baseTypeKey = "Game::Game.Entities.Entity";

    tests.expect(aggregator.addPart(actions, error), "first partial class part is accepted");
    tests.expect(aggregator.addPart(state, error), "second partial class part is accepted");
    tests.expect(aggregator.finalize(error), "compatible partial class parts merge");
    tests.expect(aggregator.types().size() == 1, "partial declarations produce one semantic type");
    if (aggregator.types().size() == 1)
    {
        const auto& type = aggregator.types().front();
        tests.expect(type.parts.size() == 2, "merged type retains both source parts");
        tests.expect(type.parts[0].sourceOrdinal == 0, "partial parts use canonical project order");
        tests.expect(type.accessibility == DeclaredAccessibility::Public, "explicit accessibility is merged");
        tests.expect(type.isSealed, "sealed modifier is the union of partial declarations");
        tests.expect(
            type.baseTypeKey == std::optional<std::string>("Game::Game.Entities.Entity"),
            "one partial declaration may provide the common base class");
    }

    PartialTypeAggregator single;
    tests.expect(single.addPart(playerPart(0, false), error), "single non-partial type declaration is accepted");
    tests.expect(single.finalize(error), "single non-partial type finalizes");
}

void testPartialAggregationNegative(TestContext& tests)
{
    std::string error;

    PartialTypeAggregator missingPartial;
    tests.expect(missingPartial.addPart(playerPart(0, false), error), "non-partial first part is collected");
    tests.expect(missingPartial.addPart(playerPart(1), error), "partial second part is collected");
    tests.expect(!missingPartial.finalize(error), "multiple declarations require partial on every part");

    PartialTypeAggregator kindConflict;
    TypePartDescriptor classPart = playerPart(0);
    TypePartDescriptor structPart = playerPart(1);
    structPart.kind = TypeDeclarationKind::Struct;
    tests.expect(kindConflict.addPart(classPart, error), "class part for kind conflict is collected");
    tests.expect(kindConflict.addPart(structPart, error), "struct part for kind conflict is collected");
    tests.expect(!kindConflict.finalize(error), "partial declarations of different kinds are rejected");

    PartialTypeAggregator accessibilityConflict;
    TypePartDescriptor publicPart = playerPart(0);
    publicPart.accessibility = DeclaredAccessibility::Public;
    TypePartDescriptor internalPart = playerPart(1);
    internalPart.accessibility = DeclaredAccessibility::Internal;
    tests.expect(accessibilityConflict.addPart(publicPart, error), "public part is collected");
    tests.expect(accessibilityConflict.addPart(internalPart, error), "internal part is collected");
    tests.expect(!accessibilityConflict.finalize(error), "conflicting partial accessibility is rejected");

    PartialTypeAggregator baseConflict;
    TypePartDescriptor firstBase = playerPart(0);
    firstBase.baseTypeKey = "Game::BaseA";
    TypePartDescriptor secondBase = playerPart(1);
    secondBase.baseTypeKey = "Game::BaseB";
    tests.expect(baseConflict.addPart(firstBase, error), "first base declaration is collected");
    tests.expect(baseConflict.addPart(secondBase, error), "second base declaration is collected");
    tests.expect(!baseConflict.finalize(error), "different partial base classes are rejected");

    PartialTypeAggregator modifierConflict;
    TypePartDescriptor abstractPart = playerPart(0);
    abstractPart.isAbstract = true;
    TypePartDescriptor sealedPart = playerPart(1);
    sealedPart.isSealed = true;
    tests.expect(modifierConflict.addPart(abstractPart, error), "abstract part is collected");
    tests.expect(modifierConflict.addPart(sealedPart, error), "sealed part is collected");
    tests.expect(!modifierConflict.finalize(error), "abstract and sealed partial class is rejected");

    PartialTypeAggregator staticModifierConflict;
    TypePartDescriptor staticPart = playerPart(0);
    staticPart.isStatic = true;
    TypePartDescriptor abstractStaticPart = playerPart(1);
    abstractStaticPart.isAbstract = true;
    tests.expect(staticModifierConflict.addPart(staticPart, error), "static part is collected");
    tests.expect(staticModifierConflict.addPart(abstractStaticPart, error), "abstract static part is collected");
    tests.expect(
        !staticModifierConflict.finalize(error),
        "static partial class cannot explicitly combine with abstract");

    PartialTypeAggregator duplicateLocation;
    TypePartDescriptor first = playerPart(0);
    TypePartDescriptor duplicate = playerPart(0);
    tests.expect(duplicateLocation.addPart(first, error), "first source location is accepted");
    tests.expect(!duplicateLocation.addPart(duplicate, error), "duplicate AST source location is rejected");

    PartialTypeAggregator crossModuleLocations;
    TypePartDescriptor gamePart = playerPart(0, false);
    TypePartDescriptor editorPart = playerPart(0, false);
    editorPart.identity = TypeIdentity("Editor", {"Game", "Entities"}, {}, "Player");
    editorPart.sourcePath = "EditorPlayer.ns";
    tests.expect(crossModuleLocations.addPart(gamePart, error),
        "first module may use source ordinal zero at offset ten");
    tests.expect(crossModuleLocations.addPart(editorPart, error),
        "another module may reuse the same source ordinal and offset without a false collision");
    tests.expect(crossModuleLocations.finalize(error),
        "module identity separates equal source locations during aggregation");
    tests.expect(crossModuleLocations.types().size() == 2,
        "equal type spelling and source coordinates in distinct modules produce distinct types");

    PartialTypeAggregator crossTypeLocations;
    TypePartDescriptor firstType = playerPart(0, false);
    TypePartDescriptor secondType = playerPart(0, false);
    secondType.identity = TypeIdentity("Game", {"Game", "Entities"}, {}, "Enemy");
    secondType.sourcePath = "SyntheticSameLocation.ns";
    tests.expect(crossTypeLocations.addPart(firstType, error),
        "first type identity may claim a synthetic source coordinate");
    tests.expect(crossTypeLocations.addPart(secondType, error),
        "a distinct type identity may reuse that coordinate without being mistaken for a duplicate part");
    tests.expect(crossTypeLocations.finalize(error),
        "full TypeIdentity participates in duplicate-part detection");
    tests.expect(crossTypeLocations.types().size() == 2,
        "distinct type identities remain separate even with identical synthetic source coordinates");
}
}

int main()
{
    TestContext tests;
    testProjectSources(tests);
    testTypeIdentity(tests);
    testPartialAggregationPositive(tests);
    testPartialAggregationNegative(tests);
    return tests.result();
}
