#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "../../src/Interop/Internal/CppCRecordMetadataCodec.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <variant>
#include <vector>

using namespace nativesharp::interop;

int main(int argc, char** argv)
{
    if (argc != 3) return 1;
    CppSessionOptions options;
    options.language = CppLanguageMode::Cpp;
    options.languageStandard = "c++20";
    options.targetTriple = "x86_64-pc-windows-msvc";
    options.driverReferencePath = "clang.exe";
    options.resourceDirectory = argv[1];
    options.workingDirectory = argv[2];
    options.input = CppBufferInput{"record-observation.cpp", R"cpp(
        namespace N { struct Plain { int x; }; struct Deleted { Deleted(const Deleted&) = delete; int x; }; }
        struct Incomplete; struct GlobalPlain { double x; int y; }; struct __declspec(align(16)) Aligned { int x; }; struct Base { int x; }; struct Derived : Base { int y; };
    )cpp"};
    CppClangSessionFactory factory;
    auto opened = factory.open(std::move(options), {});
    if (!std::holds_alternative<std::unique_ptr<CppInteropSession>>(opened)) return 2;
    std::vector<CppProjectSessions> groups(1);
    groups[0].projectFile = std::filesystem::path(argv[2]);
    groups[0].sessions.push_back(std::move(std::get<std::unique_ptr<CppInteropSession>>(opened)));
    if (!groups[0].sessions[0]) return 3;
    const auto records = groups[0].sessions[0]->recordsSnapshot();
    if (!records) return 4;
    const auto find = [&](const char* name) -> const CppRecordDeclaration* {
        for (const auto& record : *records) if (record.qualifiedName == name) return &record;
        return nullptr;
    };
    const auto* plain = find("N::Plain");
    const auto* deleted = find("N::Deleted");
    const auto* incomplete = find("Incomplete");
    if (!plain || !deleted || !incomplete) return 5;
    auto entry = CppSemanticRecordEntry{groups[0].projectFile, 0, 0, "x86_64-pc-windows-msvc", *plain,
        projectCppRecordIdentity(*plain, "Observation"), std::nullopt};
    const auto observed = observeCppRecordValueForEntry(entry, groups);
    if (!observed || !observed->traits.triviallyCopyable ||
        observed->operations.copyFromConst.status != CppSpecialMemberSelectionStatus::Selected ||
        observed->operations.copyFromConst.access != CppAccess::Public) return 6;
    if (!cppRecordValueOperationsEligible(observed->traits, observed->operations)) return 24;
    for (auto member : {&CppRecordValueOperations::copyFromConst, &CppRecordValueOperations::copyFromMutable,
                        &CppRecordValueOperations::destructor, &CppRecordValueOperations::defaultConstructor,
                        &CppRecordValueOperations::assignFromConst, &CppRecordValueOperations::assignFromMutable})
    {
        auto operations = observed->operations;
        (operations.*member).status = CppSpecialMemberSelectionStatus::Ambiguous;
        if (cppRecordValueOperationsEligible(observed->traits, operations)) return 25;
        operations = observed->operations; (operations.*member).access.reset();
        if (cppRecordValueOperationsEligible(observed->traits, operations)) return 26;
        operations = observed->operations; (operations.*member).isDeleted.reset();
        if (cppRecordValueOperationsEligible(observed->traits, operations)) return 27;
        operations = observed->operations; (operations.*member).isTrivial.reset();
        if (cppRecordValueOperationsEligible(observed->traits, operations)) return 28;
    }
    for (auto member : {&CppRecordValueTraits::triviallyCopyable, &CppRecordValueTraits::standardLayout,
                        &CppRecordValueTraits::trivialDefaultConstructor, &CppRecordValueTraits::trivialDestructor,
                        &CppRecordValueTraits::plainOldData})
    {
        auto traits = observed->traits; traits.*member = false;
        if (cppRecordValueOperationsEligible(traits, observed->operations)) return 29;
    }
    auto volatileTraits = observed->traits; volatileTraits.hasNonLiteralOrVolatileFieldsOrBases = true;
    if (cppRecordValueOperationsEligible(volatileTraits, observed->operations)) return 30;
    CppCRecordSnapshot snapshot;
    snapshot.identityKey = entry.identity->key(); snapshot.moduleName = "Observation";
    snapshot.namespaceSegments = {"N"}; snapshot.name = "Plain";
    snapshot.targetTriple = entry.targetTriple; snapshot.clangUsr = plain->clangUsr;
    snapshot.complete = snapshot.valueLayoutUsable = true;
    snapshot.sizeBits = plain->sizeBits; snapshot.alignmentBits = plain->alignmentBits;
    snapshot.fields = plain->fields; snapshot.cppValueObservation = observed;
    std::string codecError;
    std::vector<std::uint8_t> encoded, reencoded;
    std::vector<CppCRecordSnapshot> restored;
    if (!metadata_detail::encodeCRecordCatalog(std::span(&snapshot, 1), encoded, codecError) ||
        !metadata_detail::decodeCRecordCatalog(encoded, restored, codecError) || restored.size() != 1 ||
        !restored[0].cppValueObservation || restored[0].clangUsr != snapshot.clangUsr ||
        restored[0].cppValueObservation->traits != observed->traits ||
        !cppRecordValueOperationsEligible(restored[0].cppValueObservation->traits,
                                          restored[0].cppValueObservation->operations) ||
        !metadata_detail::encodeCRecordCatalog(restored, reencoded, codecError) || reencoded != encoded) return 31;
    if (groups[0].sessions[0]->cppRecordHasPlainReplaySyntax("GlobalPlain") != true ||
        groups[0].sessions[0]->cppRecordHasPlainReplaySyntax("Aligned") != false ||
        groups[0].sessions[0]->cppRecordHasPlainReplaySyntax("Derived") != false ||
        groups[0].sessions[0]->cppRecordHasPlainReplaySyntax("N::Plain") != false) return 41;
    const auto* global = find("GlobalPlain");
    if (!global) return 42;
    CppSemanticRecordEntry globalEntry{groups[0].projectFile, 0, 0, "x86_64-pc-windows-msvc", *global,
        projectCppRecordIdentity(*global, "Observation"), std::nullopt};
    const auto globalValue = observeCppRecordValueForEntry(globalEntry, groups);
    if (!globalValue) return 43;
    auto globalSnapshot = snapshot;
    globalSnapshot.identityKey = globalEntry.identity->key(); globalSnapshot.namespaceSegments.clear();
    globalSnapshot.name = "GlobalPlain"; globalSnapshot.clangUsr = global->clangUsr;
    globalSnapshot.sizeBits = global->sizeBits; globalSnapshot.alignmentBits = global->alignmentBits;
    globalSnapshot.fields = global->fields; globalSnapshot.cppValueObservation = globalValue;
    globalSnapshot.plainReplaySyntax = true;
    std::vector<std::uint8_t> globalBytes;
    std::vector<CppCRecordSnapshot> globalRestored;
    if (!metadata_detail::encodeCRecordCatalog(std::span(&globalSnapshot, 1), globalBytes, codecError) ||
        !metadata_detail::decodeCRecordCatalog(globalBytes, globalRestored, codecError) ||
        globalRestored.size() != 1 || !globalRestored[0].plainReplaySyntax ||
        !globalRestored[0].cppValueObservation) return 44;
    auto invalidGlobal = globalSnapshot; invalidGlobal.packedReplaySyntax = true;
    if (metadata_detail::encodeCRecordCatalog(std::span(&invalidGlobal, 1), globalBytes, codecError)) return 45;
    std::vector<nativesharp::frontend::CppImportedTypeDeclaration> reconstructed;
    if (!metadata_detail::reconstructCImportedTypes(restored, {}, 0, "Observation", entry.targetTriple,
                                                    "producer.nsmeta", reconstructed, codecError) ||
        reconstructed.size() != 1 || reconstructed[0].origin != nativesharp::frontend::CppImportedTypeOrigin::CppRecord ||
        reconstructed[0].identity.key() != snapshot.identityKey || reconstructed[0].clangUsr != snapshot.clangUsr ||
        !reconstructed[0].objectLayout || !reconstructed[0].cppValueTraits || !reconstructed[0].cppValueOperations ||
        !cppRecordValueOperationsEligible(*reconstructed[0].cppValueTraits, *reconstructed[0].cppValueOperations)) return 36;
    auto cOrigin = snapshot; cOrigin.cppValueObservation.reset();
    auto comparison = metadata_detail::CRecordContractComparison::Matching;
    if (!metadata_detail::compareCRecordContracts(snapshot, cOrigin, comparison, codecError) ||
        comparison != metadata_detail::CRecordContractComparison::Different) return 39;
    auto invalid = snapshot;
    invalid.cppValueObservation->operations.clangUsr = "wrong-identity";
    if (metadata_detail::encodeCRecordCatalog(std::span(&invalid, 1), reencoded, codecError)) return 32;
    invalid = snapshot; invalid.cppValueObservation->operations.assignFromMutable.isTrivial.reset();
    if (metadata_detail::encodeCRecordCatalog(std::span(&invalid, 1), reencoded, codecError)) return 33;
    invalid = snapshot; invalid.cppValueObservation->traits.hasNonLiteralOrVolatileFieldsOrBases = true;
    if (metadata_detail::encodeCRecordCatalog(std::span(&invalid, 1), reencoded, codecError)) return 34;
    auto corrupted = encoded;
    const auto& usr = *snapshot.clangUsr;
    const auto usrPos = std::search(corrupted.begin(), corrupted.end(), usr.begin(), usr.end());
    if (usrPos == corrupted.end() || static_cast<std::size_t>(corrupted.end() - usrPos) <= usr.size() + 7) return 37;
    // After the first USR: volatile mode, kind, completeness, value layout,
    // two replay flags, observation presence, then the first boolean trait.
    *(usrPos + static_cast<std::ptrdiff_t>(usr.size() + 7)) = 2;
    if (metadata_detail::decodeCRecordCatalog(corrupted, restored, codecError) || restored.size() != 1 ||
        !restored[0].cppValueObservation) return 38;
    encoded.pop_back();
    if (metadata_detail::decodeCRecordCatalog(encoded, restored, codecError) || restored.size() != 1 ||
        !restored[0].cppValueObservation) return 35;
    auto bad = entry;
    bad.targetTriple = "wrong-target";
    if (observeCppRecordValueForEntry(bad, groups)) return 7;
    bad = entry; bad.referenceIndex = 1;
    if (observeCppRecordValueForEntry(bad, groups)) return 8;
    bad = entry; bad.declaration.clangUsr = "wrong-identity";
    if (observeCppRecordValueForEntry(bad, groups)) return 20;
    bad = entry; bad.declaration = *incomplete;
    if (observeCppRecordValueForEntry(bad, groups)) return 9;
    bad = entry; bad.declaration = *deleted;
    bad.identity = projectCppRecordIdentity(*deleted, "Observation");
    const auto deletedObserved = observeCppRecordValueForEntry(bad, groups);
    if (!deletedObserved || deletedObserved->operations.copyFromConst.status !=
            CppSpecialMemberSelectionStatus::MissingOrDeleted) return 10;
    const std::vector<CppSemanticRecordEntry> inventory{entry, bad};
    const auto grouped = groupCppSemanticRecords(inventory);
    const std::vector<std::string> validSeed{entry.identity->key()};
    auto closure = collectCppCRecordClosure(inventory, grouped.groups, 0, validSeed, groups);
    const auto* captured = std::get_if<CppCRecordClosure>(&closure);
    if (!captured || captured->records.size() != 1 || !captured->records[0].cppValueObservation ||
        captured->records[0].clangUsr != plain->clangUsr) return 40;
    const std::vector<std::string> invalidSeed{bad.identity->key()};
    if (!std::holds_alternative<CppCRecordSnapshotFailure>(
            collectCppCRecordClosure(inventory, grouped.groups, 0, invalidSeed, groups)) ||
        !std::holds_alternative<CppCRecordSnapshotFailure>(
            collectCppCRecordClosure(inventory, grouped.groups, 0, validSeed))) return 41;
    groups.emplace_back(); groups.back().projectFile = groups[0].projectFile;
    if (observeCppRecordValueForEntry(entry, groups)) return 11;
    groups.pop_back();
    if (!groups[0].sessions[0]->close().succeeded || observeCppRecordValueForEntry(entry, groups)) return 12;

    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto projectDir = std::filesystem::path(argv[2]) / ("record-value-observation-" + unique);
    std::error_code ec;
    std::filesystem::create_directories(projectDir, ec);
    if (ec) return 13;
    const auto projectPath = projectDir / "Observation.nsproj";
    { std::ofstream project(projectPath); if (!project) return 14;
      project << "<Project><PropertyGroup><Name>Observation</Name></PropertyGroup>"
                 "<ItemGroup><Compile Include=\"Main.ns\" /></ItemGroup></Project>"; }
    { std::ofstream source(projectDir / "Main.ns"); if (!source) return 15;
      source << "int Use(N.Plain* x, N.Deleted* y, N.PrivateDefault* a, N.DeletedDefault* b, N.PrivateCopy* c, N.PrivateDtor* d, N.UserDtor* e, N.DeletedAssign* f, N.PrivateAssign* g, N.VolatileMember* h, N.NestedVolatile* i) { return 0; } int Main() { return 0; }\n"; }
    CppSessionOptions liveOptions;
    liveOptions.language = CppLanguageMode::Cpp;
    liveOptions.languageStandard = "c++20";
    liveOptions.targetTriple = "x86_64-pc-windows-msvc";
    liveOptions.driverReferencePath = "clang.exe";
    liveOptions.resourceDirectory = argv[1];
    liveOptions.workingDirectory = argv[2];
    liveOptions.input = CppBufferInput{"record-observation-live.cpp", R"cpp(
        namespace N { struct Plain { int x; }; struct Deleted { Deleted(const Deleted&) = delete; int x; };
            struct PrivateDefault { private: PrivateDefault() = default; public: int x; };
            struct DeletedDefault { DeletedDefault() = delete; int x; };
            struct PrivateCopy { private: PrivateCopy(const PrivateCopy&) = default; public: int x; };
            struct PrivateDtor { private: ~PrivateDtor() = default; public: int x; };
            struct UserDtor { ~UserDtor(); int x; };
            struct DeletedAssign { int x; DeletedAssign& operator=(const DeletedAssign&) = delete; };
            struct PrivateAssign { int x; private: PrivateAssign& operator=(const PrivateAssign&) = default; };
            struct VolatileMember { volatile int x; }; struct NestedVolatile { VolatileMember inner; }; }
    )cpp"};
    auto liveOpened = factory.open(std::move(liveOptions), {});
    if (!std::holds_alternative<std::unique_ptr<CppInteropSession>>(liveOpened)) return 16;
    std::vector<CppProjectSessions> liveGroups(1);
    liveGroups[0].projectFile = projectPath;
    liveGroups[0].sessions.push_back(std::move(std::get<std::unique_ptr<CppInteropSession>>(liveOpened)));
    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    if (!compilation.setCppProjectSessions(std::move(liveGroups), error) ||
        !compilation.setExperimentalCppTypeProvider(provideExperimentalCppRecordCandidates, error) ||
        !compilation.setExperimentalCppTypeSelector(selectExperimentalCppRecordTypesForCompilation, error) ||
        !compilation.compileProjectNames(projectPath, error))
    { std::cerr << "project selection: " << error << '\n'; return 17; }
    const auto* lookup = compilation.nameLookup();
    const auto plainIdentity = projectCppRecordIdentity(*plain, "Observation");
    const auto deletedIdentity = projectCppRecordIdentity(*deleted, "Observation");
    const auto plainOrdinal = lookup && plainIdentity ? lookup->typeOrdinalForIdentity(plainIdentity->key()) : std::nullopt;
    const auto deletedOrdinal = lookup && deletedIdentity ? lookup->typeOrdinalForIdentity(deletedIdentity->key()) : std::nullopt;
    const auto* plainType = plainOrdinal ? lookup->cppImportedType(*plainOrdinal) : nullptr;
    const auto* deletedType = deletedOrdinal ? lookup->cppImportedType(*deletedOrdinal) : nullptr;
    if (!plainType || !plainType->cppValueTraits || !plainType->cppValueOperations ||
        !plainType->cppValueTraits->triviallyCopyable ||
        plainType->cppValueOperations->copyFromConst.status != CppSpecialMemberSelectionStatus::Selected) return 18;
    if (!deletedType || !deletedType->cppValueTraits || !deletedType->cppValueOperations ||
        deletedType->cppValueOperations->copyFromConst.status != CppSpecialMemberSelectionStatus::MissingOrDeleted) return 19;
    if (!lookup->cppTrivialValueEligible(*plainOrdinal) || lookup->cppTrivialValueEligible(*deletedOrdinal)) return 21;
    const auto privateDefaultOrdinals = lookup->typeOrdinalsForName("PrivateDefault");
    const auto deletedDefaultOrdinals = lookup->typeOrdinalsForName("DeletedDefault");
    if (privateDefaultOrdinals.size() != 1 || deletedDefaultOrdinals.size() != 1 ||
        lookup->cppTrivialValueEligible(privateDefaultOrdinals[0]) ||
        lookup->cppTrivialValueEligible(deletedDefaultOrdinals[0])) return 22;
    for (const auto* name : {"PrivateCopy", "PrivateDtor", "UserDtor", "DeletedAssign", "PrivateAssign", "VolatileMember", "NestedVolatile"})
    {
        const auto ordinals = lookup->typeOrdinalsForName(name);
        if (ordinals.size() != 1 || lookup->cppTrivialValueEligible(ordinals[0])) return 23;
    }
    return 0;
}
