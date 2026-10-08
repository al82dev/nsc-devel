#include "nsc_test/Modules.h"

#include "nsc_test/TestContext.h"
#include "nsc_test/TestRegistry.h"
#include "nativesharp/Basic/SourceLocation.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Source/SourceManager.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace nsc::test
{
namespace
{
void verifySourceManager(TestContext& context)
{
    using namespace nativesharp;
    using namespace nativesharp::source;

    SourceManager manager;
    const FileId emptyId = manager.addBuffer("empty.ns", "");
    const SourceFile* empty = manager.getFile(emptyId);
    context.expect(empty != nullptr, "empty source is registered");
    context.expect(empty != nullptr && empty->size() == 0, "empty source has zero bytes");
    context.expect(
        empty != nullptr && empty->lineCount() == 1,
        "empty source has one logical line");

    const std::string mixed = "alpha\r\nbeta\ngamma\rdelta";
    const FileId id = manager.addBuffer("mixed.ns", mixed);
    const SourceFile* file = manager.getFile(id);
    context.expect(file != nullptr, "mixed-line-ending source is registered");
    context.expect(
        file != nullptr && file->lineCount() == 4,
        "LF, CRLF, and CR create logical lines");
    const SourcePosition p0 = manager.getPosition(SourceLocation(id, 0));
    context.expect(p0.line == 1 && p0.column == 1, "offset zero maps to line 1 column 1");
    const SourcePosition pBeta = manager.getPosition(SourceLocation(id, 7));
    context.expect(
        pBeta.line == 2 && pBeta.column == 1,
        "offset after CRLF maps to next line");
    const SourcePosition pGamma = manager.getPosition(SourceLocation(id, 12));
    context.expect(
        pGamma.line == 3 && pGamma.column == 1,
        "offset after LF maps to next line");
    const SourcePosition pDelta = manager.getPosition(SourceLocation(id, 18));
    context.expect(
        pDelta.line == 4 && pDelta.column == 1,
        "offset after CR maps to next line");
    context.expect(file != nullptr && file->lineText(1) == "alpha", "lineText strips CRLF");
    context.expect(file != nullptr && file->lineText(2) == "beta", "lineText strips LF");
    context.expect(file != nullptr && file->lineText(3) == "gamma", "lineText strips CR");
    context.expect(file != nullptr && file->lineText(4) == "delta", "last line has no terminator");

    const SourceRange betaRange(SourceLocation(id, 7), SourceLocation(id, 11));
    context.expect(manager.getText(betaRange) == "beta", "source range returns exact bytes");
    context.expect(
        manager.getPath(SourceLocation(id, 0)) == "mixed.ns",
        "location resolves display path");
    context.expect(
        manager.getLineText(SourceLocation(id, 14)) == "gamma",
        "location resolves line text");

    const FileId invalid;
    context.expect(manager.getFile(invalid) == nullptr, "invalid FileId does not resolve");
    context.expect(!SourceLocation().isValid(), "default SourceLocation is invalid");
    context.expect(!SourceRange().isValid(), "default SourceRange is invalid");
    const SourcePosition eofPosition = manager.getPosition(
        SourceLocation(id, static_cast<std::uint32_t>(mixed.size())));
    context.expect(
        eofPosition.line == 4 && eofPosition.column == 6,
        "EOF position maps one column past final byte");

    const FileId trailingId = manager.addBuffer("trailing.ns", "one\n");
    const SourceFile* trailing = manager.getFile(trailingId);
    context.expect(
        trailing != nullptr && trailing->lineCount() == 2,
        "terminal newline creates a final empty logical line");
    context.expect(
        trailing != nullptr && trailing->lineText(2).empty(),
        "final logical line after newline is empty");
    const SourceRange crossFile(SourceLocation(id, 0), SourceLocation(trailingId, 1));
    context.expect(!crossFile.isValid(), "SourceRange cannot cross files");
    context.expect(
        manager.getText(SourceRange(SourceLocation(id, 10), SourceLocation(id, 9))).empty(),
        "invalid reversed range produces no text");

    const std::filesystem::path fixture =
        std::filesystem::path(NSC_TEST_CASES_DIR) /
        "unit-data/source-manager/disk-source.ns";
    std::string loadError;
    const std::optional<FileId> loadedId = manager.loadFile(fixture, loadError);
    context.expect(loadedId.has_value(), "SourceManager loads a real source file from disk");
    if (loadedId.has_value())
    {
        const SourceFile* loaded = manager.getFile(*loadedId);
        context.expect(
            loaded != nullptr && loaded->lineCount() == 5,
            "disk-loaded source indexes lines correctly");
        context.expect(
            manager.getPath(SourceLocation(*loadedId, 0)) == fixture.string(),
            "disk-loaded source preserves its path");
    }

    std::string missingError;
    const std::filesystem::path missingPath = fixture.string() + ".missing";
    const std::optional<FileId> missingId = manager.loadFile(missingPath, missingError);
    context.expect(!missingId.has_value(), "missing source file is rejected");
    context.expect(
        missingError.find("cannot open source file") != std::string::npos,
        "missing source file reports a deterministic load error");
}
}

void registerSourceUnitTests(TestRegistry& registry)
{
    registry.add(TestCase{
        "nsc.source-manager",
        "source",
        "source manager positions and files",
        {"source", "locations", "files"},
        std::chrono::milliseconds(1000),
        verifySourceManager,
    });
}
}
