#include "nativesharp/Frontend/PrecompiledMemberIdentityIndex.h"

#include "../support/TestContext.h"

#include <vector>

int main()
{
    using nativesharp::frontend::ClassMemberKind;
    using nativesharp::frontend::PrecompiledMemberIdentity;
    using nativesharp::frontend::PrecompiledMemberIdentityIndex;
    using nativesharp::frontend::PrecompiledMemberIdentityMatchKind;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const std::vector<PrecompiledMemberIdentity> identities = {
        {ClassMemberKind::Method, "type:Math.Vector", "Add", "Add(int)", "?add_int"},
        {ClassMemberKind::Method, "type:Math.Vector", "Add", "Add(float)", "?add_float"},
        {ClassMemberKind::Constructor, "type:Math.Vector", ".ctor", ".ctor(int)", "?ctor"},
        {ClassMemberKind::Method, "type:Math.Vector", "Duplicate", "Duplicate()", "?dup1"},
        {ClassMemberKind::Method, "type:Math.Vector", "Duplicate", "Duplicate()", "?dup2"},
        {ClassMemberKind::Method, "type:Math.Vector", "Ignored", "Ignored()", ""}};

    const PrecompiledMemberIdentityIndex index(identities);
    const auto exact = index.find(
        ClassMemberKind::Method, "type:Math.Vector", "Add", "Add(float)");
    const auto missing = index.find(
        ClassMemberKind::Method, "type:Math.Vector", "Add", "Add(double)");
    const auto ambiguous = index.find(
        ClassMemberKind::Method, "type:Math.Vector", "Duplicate", "Duplicate()");

    tests.expect(exact.kind == PrecompiledMemberIdentityMatchKind::Unique &&
            exact.identity != nullptr && exact.identity->mangledName == "?add_float",
        "indexed lookup returns the exact overload identity");
    tests.expect(missing.kind == PrecompiledMemberIdentityMatchKind::Missing &&
            missing.identity == nullptr,
        "indexed lookup distinguishes a missing signature");
    tests.expect(ambiguous.kind == PrecompiledMemberIdentityMatchKind::Ambiguous &&
            ambiguous.identity == nullptr,
        "indexed lookup preserves duplicate-identity diagnostics");
    tests.expect(index.indexedIdentityCount() == 5U && index.lookupCount() == 3U,
        "the table indexes every publishable identity once and performs one lookup per callable");

    return tests.result();
}
