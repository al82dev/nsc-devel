#include "nativesharp/Basic/SourceLocation.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>

int main()
{
    using namespace nativesharp;
    using namespace nativesharp::source;
    using nativesharp::tests::TestContext;

    TestContext tests;
    SourceManager manager;

    const FileId emptyId = manager.addBuffer("empty.ns", "");
    const SourceFile* empty = manager.getFile(emptyId);
    tests.expect(empty != nullptr, "empty source is registered");
    tests.expect(empty != nullptr && empty->size() == 0, "empty source has zero bytes");
    tests.expect(empty != nullptr && empty->lineCount() == 1, "empty source has one logical line");

    const std::string mixed = "alpha\r\nbeta\ngamma\rdelta";
    const FileId id = manager.addBuffer("mixed.ns", mixed);
    const SourceFile* file = manager.getFile(id);
    tests.expect(file != nullptr, "mixed-line-ending source is registered");
    tests.expect(file != nullptr && file->lineCount() == 4, "LF, CRLF, and CR create logical lines");

    const SourcePosition p0 = manager.getPosition(SourceLocation(id, 0));
    tests.expect(p0.line == 1 && p0.column == 1, "offset zero maps to line 1 column 1");

    const SourcePosition pBeta = manager.getPosition(SourceLocation(id, 7));
    tests.expect(pBeta.line == 2 && pBeta.column == 1, "offset after CRLF maps to next line");

    const SourcePosition pGamma = manager.getPosition(SourceLocation(id, 12));
    tests.expect(pGamma.line == 3 && pGamma.column == 1, "offset after LF maps to next line");

    const SourcePosition pDelta = manager.getPosition(SourceLocation(id, 18));
    tests.expect(pDelta.line == 4 && pDelta.column == 1, "offset after CR maps to next line");

    tests.expect(file != nullptr && file->lineText(1) == "alpha", "lineText strips CRLF");
    tests.expect(file != nullptr && file->lineText(2) == "beta", "lineText strips LF");
    tests.expect(file != nullptr && file->lineText(3) == "gamma", "lineText strips CR");
    tests.expect(file != nullptr && file->lineText(4) == "delta", "last line has no terminator");

    const SourceRange betaRange(SourceLocation(id, 7), SourceLocation(id, 11));
    tests.expect(manager.getText(betaRange) == "beta", "source range returns exact bytes");
    tests.expect(manager.getPath(SourceLocation(id, 0)) == "mixed.ns", "location resolves display path");
    tests.expect(manager.getLineText(SourceLocation(id, 14)) == "gamma", "location resolves line text");

    const FileId invalid;
    tests.expect(manager.getFile(invalid) == nullptr, "invalid FileId does not resolve");
    tests.expect(!SourceLocation().isValid(), "default SourceLocation is invalid");
    tests.expect(!SourceRange().isValid(), "default SourceRange is invalid");

    const SourcePosition eofPosition = manager.getPosition(
        SourceLocation(id, static_cast<std::uint32_t>(mixed.size())));
    tests.expect(eofPosition.line == 4 && eofPosition.column == 6, "EOF position maps one column past final byte");

    const FileId trailingId = manager.addBuffer("trailing.ns", "one\n");
    const SourceFile* trailing = manager.getFile(trailingId);
    tests.expect(
        trailing != nullptr && trailing->lineCount() == 2,
        "terminal newline creates a final empty logical line");
    tests.expect(trailing != nullptr && trailing->lineText(2).empty(), "final logical line after newline is empty");

    const SourceRange crossFile(SourceLocation(id, 0), SourceLocation(trailingId, 1));
    tests.expect(!crossFile.isValid(), "SourceRange cannot cross files");
    tests.expect(manager.getText(SourceRange(SourceLocation(id, 10), SourceLocation(id, 9))).empty(),
        "invalid reversed range produces no text");

    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "nativesharp_source_manager_stage07.ns";
    {
        std::ofstream stream(tempPath, std::ios::binary);
        stream << "int Main()\r\n{\r\n    return 0;\r\n}\r\n";
    }
    std::string loadError;
    const std::optional<FileId> loadedId = manager.loadFile(tempPath, loadError);
    tests.expect(loadedId.has_value(), "SourceManager loads a real source file from disk");
    if (loadedId.has_value())
    {
        const SourceFile* loaded = manager.getFile(*loadedId);
        tests.expect(loaded != nullptr && loaded->lineCount() == 5, "disk-loaded CRLF source indexes lines correctly");
        tests.expect(manager.getPath(SourceLocation(*loadedId, 0)) == tempPath.string(),
            "disk-loaded source preserves its path");
    }
    std::filesystem::remove(tempPath);

    std::string missingError;
    const std::filesystem::path missingPath = tempPath.string() + ".missing";
    const std::optional<FileId> missingId = manager.loadFile(missingPath, missingError);
    tests.expect(!missingId.has_value(), "missing source file is rejected");
    tests.expect(missingError.find("cannot open source file") != std::string::npos,
        "missing source file reports a deterministic load error");

    return tests.result();
}
