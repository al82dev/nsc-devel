#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
import time
from pathlib import Path


SCHEMA = "nativesharp-selfhost-first-generation-contract"
REPORT_SCHEMA = "nativesharp-selfhost-sh10-report"


class VerificationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_files(root: Path, contract: dict, key: str = "source_roots") -> list[Path]:
    result: list[Path] = []
    for relative in contract[key]:
        directory = root / relative
        require(directory.is_dir(), f"missing source root: {relative}")
        result.extend(path for path in directory.rglob("*") if path.suffix in {".ns", ".nsproj"})
    return sorted(set(result), key=lambda path: path.relative_to(root).as_posix())


def manifest(root: Path, contract: dict, key: str = "source_roots") -> tuple[list[dict[str, object]], str]:
    entries = [
        {
            "path": path.relative_to(root).as_posix(),
            "size": path.stat().st_size,
            "sha256": digest(path),
        }
        for path in source_files(root, contract, key)
    ]
    canonical = "".join(f"{entry['path']}\0{entry['size']}\0{entry['sha256']}\n" for entry in entries)
    return entries, hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def run(executable: Path, *arguments: str) -> tuple[subprocess.CompletedProcess[str], float]:
    started = time.perf_counter()
    completed = subprocess.run(
        [str(executable), *arguments],
        text=True,
        encoding="utf-8",
        errors="strict",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    return completed, time.perf_counter() - started


def pe_contract(path: Path, required_imports: list[str]) -> dict[str, object]:
    data = path.read_bytes()
    require(len(data) >= 0x100 and data[:2] == b"MZ", f"{path.name} is not a PE image")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    require(data[pe:pe + 4] == b"PE\0\0", f"{path.name} misses PE signature")
    machine, sections, timestamp = struct.unpack_from("<HHI", data, pe + 4)
    require(machine == 0x8664 and sections >= 4, f"{path.name} is not AMD64 PE")
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    optional = pe + 24
    require(optional_size >= 0xF0 and struct.unpack_from("<H", data, optional)[0] == 0x20B,
            f"{path.name} is not PE32+")
    imports = [name for name in required_imports if name.encode("ascii") in data]
    require(imports == required_imports, f"{path.name} import surface changed: {imports}")
    return {"machine": "AMD64", "sections": sections, "timestamp": timestamp, "imports": imports}


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify NativeSharp first generation nsc1.")
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    # EN: The NativeSharp Libraries checkout; each clean root keeps its copy beside the compiler tree, in
    #     <clean root>/../nativesharp-libraries, where lib/<library>/*.nsproj forward.
    # ES: El checkout de NativeSharp Libraries; cada raiz limpia guarda su copia junto al arbol del compilador,
    #     en <raiz limpia>/../nativesharp-libraries, adonde redirigen lib/<library>/*.nsproj.
    parser.add_argument("--libraries-root", type=Path, required=True)
    parser.add_argument("--clean-a-root", type=Path, required=True)
    parser.add_argument("--clean-b-root", type=Path, required=True)
    parser.add_argument("--nsc0", type=Path, required=True)
    parser.add_argument("--nsc1-a", type=Path, required=True)
    parser.add_argument("--nsc1-b", type=Path, required=True)
    parser.add_argument("--nslink", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        contract = json.loads(arguments.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == SCHEMA and contract.get("version") == 1,
                "unsupported SH10 contract")
        require(len(contract.get("work_items", [])) == 12, "incomplete SH10 ledger")
        arguments.output_dir.mkdir(parents=True, exist_ok=True)

        source_entries, source_digest = manifest(arguments.source_root, contract)
        clean_a_entries, clean_a_digest = manifest(arguments.clean_a_root, contract)
        clean_b_entries, clean_b_digest = manifest(arguments.clean_b_root, contract)
        require(source_entries == clean_a_entries == clean_b_entries,
                "clean-tree source manifests differ from the frozen source set")
        require(source_digest == clean_a_digest == clean_b_digest,
                "clean-tree source digests differ")
        library_entries, library_digest = manifest(arguments.libraries_root, contract, "library_source_roots")
        require(bool(library_entries), "the standard library source set is empty")
        clean_a_libraries = arguments.clean_a_root.parent / "nativesharp-libraries"
        clean_b_libraries = arguments.clean_b_root.parent / "nativesharp-libraries"
        clean_a_library_entries, clean_a_library_digest = manifest(clean_a_libraries, contract, "library_source_roots")
        clean_b_library_entries, clean_b_library_digest = manifest(clean_b_libraries, contract, "library_source_roots")
        require(library_entries == clean_a_library_entries == clean_b_library_entries,
                "clean-tree standard library manifests differ from the frozen library set")
        require(library_digest == clean_a_library_digest == clean_b_library_digest,
                "clean-tree standard library digests differ")

        first_hash = digest(arguments.nsc1_a)
        second_hash = digest(arguments.nsc1_b)
        require(first_hash == second_hash, "nsc1 differs between independent absolute paths")
        pe = pe_contract(arguments.nsc1_a, contract["required_imports"])
        require(pe_contract(arguments.nsc1_b, contract["required_imports"]) == pe,
                "nsc1 PE contracts differ")
        require(arguments.nslink.is_file(), "NSLink is not physical")

        timings: dict[str, float] = {}
        version, timings["version"] = run(arguments.nsc1_a, "--version")
        require(version.returncode == 0 and version.stdout.strip() == contract["expected_identity"],
                "nsc1 identity changed")
        help_result, timings["help"] = run(arguments.nsc1_a, "--help")
        require(help_result.returncode == 0 and "Usage:" in help_result.stdout, "nsc1 help failed")
        self_test, timings["self_test"] = run(arguments.nsc1_a, "--self-test-basic")
        require(self_test.returncode == 42, "nsc1 basic self-test failed")

        missing = arguments.output_dir / "missing source.ns"
        nsc0_missing, _ = run(arguments.nsc0, "--no-banner", str(missing))
        nsc1_missing, timings["diagnostic"] = run(arguments.nsc1_a, str(missing))
        require((nsc0_missing.returncode, nsc0_missing.stdout, nsc0_missing.stderr) ==
                (nsc1_missing.returncode, nsc1_missing.stdout, nsc1_missing.stderr),
                "nsc1 missing-source diagnostic differs from nsc0")

        compiler_sources = sorted((arguments.source_root / "selfhost/nsc").rglob("*.ns"))
        require(len(compiler_sources) >= 20, "compiler source set unexpectedly small")
        subsystem_checks = 0
        # EN: A compiler source is not a program on its own: it names declarations of other compiler files and of
        #     the standard libraries, and nsc0 rejects it as a lone file too. nsc1 runs lex, parse, semantics and
        #     HIR/MIR/CFG/dominance/SSA over the compiler's whole source graph, and the graph must hold every
        #     compiler source, so each one is still checked, now with the declarations it depends on.
        # ES: Una fuente del compilador no es un programa por si sola: nombra declaraciones de otros archivos del
        #     compilador y de las bibliotecas estandar, y nsc0 tambien la rechaza como archivo suelto. nsc1 ejecuta
        #     lexer, parser, semantica y HIR/MIR/CFG/dominancia/SSA sobre todo el grafo de fuentes del compilador,
        #     y el grafo debe contener cada fuente del compilador, asi cada una se sigue comprobando, ahora con las
        #     declaraciones de las que depende.
        compiler_project = arguments.source_root / contract["compiler_project"]
        graph, _ = run(arguments.nsc1_a, "--dump-source-graph", str(compiler_project))
        require(graph.returncode == 0 and "status=ok" in graph.stdout, "nsc1 could not load the compiler graph")
        compiler_owner = re.search(r"<Name>([^<]+)</Name>", compiler_project.read_text(encoding="utf-8"))
        require(compiler_owner is not None, "compiler project has no name")
        graph_sources = sorted(
            fields[3] for fields in (line.split(" ", 3) for line in graph.stdout.splitlines())
            if len(fields) == 4 and fields[0] == "unit" and fields[2] == compiler_owner.group(1))
        expected_sources = sorted(source.relative_to(arguments.source_root / "selfhost/nsc").as_posix()
                                  for source in compiler_sources)
        require(graph_sources == expected_sources, "compiler source graph does not hold every compiler source")
        result, elapsed = run(arguments.nsc1_a, "--verify-ssa", str(compiler_project))
        require(result.returncode == 0, f"nsc1 rejected the compiler source graph: {result.stderr}")
        timings["ssa:compiler-graph"] = elapsed
        subsystem_checks += len(graph_sources)
        # EN: Negative controls: a lone compiler source that imports another compiler namespace is rejected by
        #     nsc1 as by nsc0, and so is a project with an unresolved name.
        # ES: Controles negativos: una fuente suelta del compilador que importa otro espacio de nombres del
        #     compilador se rechaza en nsc1 como en nsc0, y tambien un proyecto con un nombre sin resolver.
        dependent = next((source for source in compiler_sources
                          if "using NativeSharp.Compiler." in source.read_text(encoding="utf-8")), None)
        require(dependent is not None, "no compiler source imports another compiler namespace")
        lone, _ = run(arguments.nsc1_a, "--verify-ssa", str(dependent))
        nsc0_lone, _ = run(arguments.nsc0, "--verify-ssa", str(dependent))
        require(lone.returncode != 0 and nsc0_lone.returncode != 0,
                f"a lone dependent compiler source was accepted: {dependent.name}")
        negative_root = arguments.output_dir / "unresolved-project"
        negative_root.mkdir(parents=True, exist_ok=True)
        core_project = (arguments.source_root / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").as_posix()
        (negative_root / "Unresolved.nsproj").write_text(
            "<Project><PropertyGroup><Name>Unresolved</Name><OutputType>Exe</OutputType></PropertyGroup>"
            "<ItemGroup><Compile Include=\"Main.ns\" /><ProjectReference Include=\"" + core_project +
            "\" /></ItemGroup></Project>\n", encoding="utf-8")
        (negative_root / "Main.ns").write_text(
            "public class P { public static int Main() { return MissingType.Value(); } }\n", encoding="utf-8")
        unresolved, _ = run(arguments.nsc1_a, "--verify-ssa", str(negative_root / "Unresolved.nsproj"))
        require(unresolved.returncode != 0, "nsc1 accepted a project with an unresolved name")

        for project in contract["profile_projects"]:
            result, elapsed = run(arguments.nsc1_a, "--verify-project", str(arguments.source_root / project))
            require(result.returncode == 0, f"nsc1 rejected bootstrap project {project}")
            timings[f"project:{project}"] = elapsed
            subsystem_checks += 1

        artifact_hashes: dict[str, str] = {}
        executions = 0
        for profile in contract["optimizations"]:
            lower = profile.lower()
            obj = arguments.output_dir / f"corpus-{lower}.obj"
            require(obj.is_file() and obj.stat().st_size > 0, f"missing corpus object {profile}")
            artifact_hashes[obj.name] = digest(obj)
            for route in ("nslink",):
                executable = arguments.output_dir / f"{route}-{lower}.exe"
                require(executable.is_file(), f"missing {route}/{profile} executable")
                artifact_hashes[executable.name] = digest(executable)
                completed, elapsed = run(executable)
                require(completed.returncode == contract["expected_exit_code"],
                        f"{route}/{profile} exited {completed.returncode}")
                timings[f"execute:{route}:{profile}"] = elapsed
                executions += 1

        report = {
            "schema": REPORT_SCHEMA,
            "version": 1,
            "status": "passed",
            "generation": "nsc1",
            "built_by": "nsc0",
            "source_files": len(source_entries),
            "source_sha256": source_digest,
            "library_files": len(library_entries),
            "library_sha256": library_digest,
            "source_manifest": source_entries,
            "nsc0_sha256": digest(arguments.nsc0),
            "nsc1_sha256": first_hash,
            "clean_roots": 2,
            "byte_identical_clean_builds": True,
            "pe": pe,
            "compiler_sources_verified": len(compiler_sources),
            "subsystem_checks": subsystem_checks,
            "profiles": len(contract["optimizations"]),
            "linkers": len(contract["linkers"]),
            "executions": executions,
            "binary_size": arguments.nsc1_a.stat().st_size,
            "timings_seconds": {key: round(value, 6) for key, value in sorted(timings.items())},
            "artifacts": dict(sorted(artifact_hashes.items())),
            "work_items": contract["work_items"],
        }
        arguments.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, UnicodeError, json.JSONDecodeError, VerificationError) as error:
        print(f"verify_first_generation: error: {error}", file=sys.stderr)
        return 1
    print("SH10 nsc1 verified: two clean roots, frozen sources, compiler corpus, O0-O3, NSLink")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
