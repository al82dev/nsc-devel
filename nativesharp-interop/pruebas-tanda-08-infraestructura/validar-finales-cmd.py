"""T08-D08: versioned Windows batch files (.cmd/.bat) are CRLF in the working tree and on every checkout.

cmd.exe locates `goto`/`call :label` targets by re-reading the file in blocks; with LF line endings it can miss a
label ("The system cannot find the batch label specified"), as happened with
pruebas-tanda-08-identidad-c/validar.cmd in the integral gate. `.gitattributes` keeps `* text=auto` (LF in the index)
and adds `*.cmd`/`*.bat text eol=crlf`, so a checkout produces CRLF independently of `core.autocrlf` and
`core.eol` (Git for Windows already writes CRLF with its defaults; Git elsewhere, or with `core.eol=lf`, wrote LF,
and tools that rewrite files left 176 versioned .cmd in LF).

Positive checks on the repository:
- every tracked .cmd/.bat has the attribute `eol: crlf`;
- the working-tree copy has CRLF line endings only;
- the content git produces for a checkout (`git archive --worktree-attributes`) is CRLF even with
  `core.autocrlf=false` and `core.eol=lf`, which every git call of this test uses.

Negative and control checks in a scratch repository:
- with only `* text=auto`, the checkout of a .cmd is LF (the old failure mode);
- with the new rules the same blob checks out with CRLF, a fresh clone writes CRLF, and `git status` stays clean;
- the working-tree checker reports a .cmd with LF and one with mixed endings.
"""
from pathlib import Path
import io
import subprocess
import sys
import tarfile
import tempfile

repo = Path(__file__).resolve().parents[2]
NEW_RULES = "*.cmd text eol=crlf\n*.bat text eol=crlf\n"


def git(args, cwd, data=None, binary=False):
    result = subprocess.run(["git", "-c", "core.autocrlf=false", "-c", "core.eol=lf", *args], cwd=cwd, input=data,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=not binary)
    if result.returncode:
        raise AssertionError(f"git {args}: {result.stderr}")
    return result.stdout


def bad_endings(data):
    """Returns 'lf' or 'mixed' when `data` has a line feed not preceded by a carriage return, else None."""
    bare = data.count(b"\n") - data.count(b"\r\n")
    if bare == 0:
        return None
    return "mixed" if b"\r\n" in data else "lf"


def in_head(cwd, path):
    return subprocess.run(["git", "cat-file", "-e", f"HEAD:{path}"], cwd=cwd, stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL).returncode == 0


def batch_files(cwd):
    return [p for p in git(["ls-files", "-z"], cwd).split("\0") if p.lower().endswith((".cmd", ".bat"))]


def checkout_contents(cwd, paths):
    """Content of each path as a checkout would write it: `git archive` applies the same conversion (with the
    working-tree attributes) in one process."""
    raw = git(["archive", "--worktree-attributes", "--format=tar", "HEAD", "--", ":(icase)*.cmd", ":(icase)*.bat"],
              cwd, binary=True)
    contents = {}
    with tarfile.open(fileobj=io.BytesIO(raw)) as archive:
        for member in archive.getmembers():
            if member.isfile():
                contents[member.name] = archive.extractfile(member).read()
    missing = [p for p in paths if p not in contents]
    if missing:
        raise AssertionError(f"git archive no devolvió {missing[:5]}")
    return contents


def check_repository(cwd):
    paths = batch_files(cwd)
    if not paths:
        raise AssertionError("no hay archivos .cmd/.bat versionados")
    attributes = git(["check-attr", "--stdin", "-z", "eol"], cwd, "\0".join(paths) + "\0").split("\0")
    eol = {attributes[i]: attributes[i + 2] for i in range(0, len(attributes) - 2, 3)}
    wrong_attribute = [p for p in paths if eol.get(p) != "crlf"]
    wrong_tree = [(p, kind) for p in paths if (kind := bad_endings((cwd / p).read_bytes()))]
    committed = [p for p in paths if in_head(cwd, p)]
    wrong_checkout = [(p, kind) for p, data in checkout_contents(cwd, committed).items()
                      if (kind := bad_endings(data))]
    return paths, wrong_attribute, wrong_tree, wrong_checkout


def main():
    paths, wrong_attribute, wrong_tree, wrong_checkout = check_repository(repo)
    if wrong_attribute or wrong_tree or wrong_checkout:
        raise AssertionError(f"atributo eol distinto de crlf: {wrong_attribute[:10]}\n"
                             f"árbol de trabajo no CRLF: {wrong_tree[:10]}\n"
                             f"extracción no CRLF: {wrong_checkout[:10]}")
    print(f"repositorio: {len(paths)} .cmd/.bat versionados con eol=crlf, árbol CRLF y extracción CRLF", flush=True)

    scratch = Path(tempfile.mkdtemp(prefix="t08-finales-cmd-", dir=r"A:\nsc-interop-build"))
    origin = scratch / "origen"
    origin.mkdir()
    git(["init", "-q"], origin)
    git(["config", "user.email", "t08@example.invalid"], origin)
    git(["config", "user.name", "T08"], origin)
    (origin / ".gitattributes").write_bytes(b"* text=auto\n")
    label_script = b"@echo off\ncall :setup\nexit /b %ERRORLEVEL%\n:setup\nexit /b 0\n"
    (origin / "validar.cmd").write_bytes(label_script)
    (origin / "build.bat").write_bytes(label_script)
    git(["add", "."], origin)
    git(["commit", "-q", "-m", "lf"], origin)
    old = checkout_contents(origin, ["validar.cmd", "build.bat"])
    if any(bad_endings(data) != "lf" for data in old.values()):
        raise AssertionError(f"control: con solo text=auto se esperaba LF: {old}")
    print("control: con solo `* text=auto` la extracción de .cmd/.bat es LF", flush=True)

    (origin / ".gitattributes").write_bytes(b"* text=auto\n" + NEW_RULES.encode())
    git(["add", ".gitattributes"], origin)
    git(["commit", "-q", "-m", "crlf"], origin)
    new = checkout_contents(origin, ["validar.cmd", "build.bat"])
    if any(bad_endings(data) for data in new.values()) or any(b"\r\n" not in data for data in new.values()):
        raise AssertionError(f"con eol=crlf se esperaba CRLF: {new}")
    clone = scratch / "clon"
    subprocess.run(["git", "-c", "core.autocrlf=false", "-c", "core.eol=lf", "clone", "-q", str(origin), str(clone)],
                   check=True)
    git(["config", "core.autocrlf", "false"], clone)
    git(["config", "core.eol", "lf"], clone)
    for name in ("validar.cmd", "build.bat"):
        if bad_endings((clone / name).read_bytes()):
            raise AssertionError(f"el clon escribió {name} sin CRLF")
    status = git(["status", "--porcelain"], clone)
    if status.strip():
        raise AssertionError(f"el clon no queda limpio: {status}")
    code = subprocess.run(["cmd", "/d", "/c", str(clone / "validar.cmd")], cwd=clone).returncode
    if code:
        raise AssertionError(f"validar.cmd del clon devolvió {code}")
    print("positivo: con eol=crlf la extracción y un clon nuevo son CRLF, el clon queda limpio y el .cmd corre",
          flush=True)

    (clone / "lf.cmd").write_bytes(b"@echo off\nexit /b 0\n")
    (clone / "mixto.cmd").write_bytes(b"@echo off\r\nexit /b 0\n")
    git(["add", "lf.cmd", "mixto.cmd"], clone)
    _, _, wrong_tree, _ = check_repository(clone)
    if sorted(wrong_tree) != [("lf.cmd", "lf"), ("mixto.cmd", "mixed")]:
        raise AssertionError(f"el verificador debía señalar lf.cmd y mixto.cmd: {wrong_tree}")
    print("negativo: el verificador señala un .cmd LF y uno mixto en el árbol de trabajo", flush=True)
    print("Finales de línea de .cmd/.bat verificados:", scratch)


if __name__ == "__main__":
    try:
        main()
    except AssertionError as failure:
        print(failure, file=sys.stderr)
        sys.exit(1)
