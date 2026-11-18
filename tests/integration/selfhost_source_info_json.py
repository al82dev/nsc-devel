"""Self-hosted --dump-source-info writes its path as an escaped JSON string (LKF-219).

The path is a directory whose name holds spaces, non-ASCII letters and JSON-neutral punctuation; the
output must parse as JSON and give back the normalized path, with the bytes read and the line count
(the empty line after the final newline counts, as in diagnostics).
"""
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="nsc-selfhost-source-info-") as temporary:
        folder = Path(temporary) / "a b ñ 'x' %&#[1]{2}"
        folder.mkdir()
        source = folder / "Main.ns"
        source.write_bytes(b"class Program {\npublic static int Main() { return 0; }\n}\n")
        result = subprocess.run([compiler, "--dump-source-info", str(source)], cwd=temporary,
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
        assert result.returncode == 0, (result.stdout, result.stderr)
        document = json.loads(result.stdout.strip())
        assert document["schema"] == "nativesharp-source-info", document
        assert Path(document["path"]) == source.resolve(), (document["path"], str(source))
        assert "\\" not in document["path"], document["path"]
        assert document["bytes"] == len(source.read_bytes()) and document["lines"] == 4, document
    print("selfhost source info json: PASS")


if __name__ == "__main__":
    main()
