#!/usr/bin/env python3
"""Certify observable nsc0/nsc2 equivalence after the first real self-compilation."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
from pathlib import Path


SCHEMA = "nativesharp-selfhost-functional-equivalence-contract"
REPORT_SCHEMA = "nativesharp-selfhost-sh42-report"


class VerificationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str], timeout: int = 180) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          check=False, timeout=timeout)


def checked(command: list[str], label: str, timeout: int = 180) -> subprocess.CompletedProcess[bytes]:
    result = run(command, timeout)
    require(result.returncode == 0,
            f"{label} failed ({result.returncode}): "
            f"{result.stdout.decode(errors='replace')}{result.stderr.decode(errors='replace')}")
    return result


def canonical(data: bytes) -> bytes:
    return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def call_verifier(python: Path, script: Path, arguments: list[str], report: Path) -> dict:
    checked([str(python), "-B", str(script), *arguments, "--report", str(report)], script.name, 900)
    value = json.loads(report.read_text(encoding="utf-8"))
    require(value.get("status") == "passed", f"{script.name} did not report passed")
    return value


def coff_summary(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    require(len(data) >= 20, f"truncated COFF object: {path}")
    machine, section_count, _, symbol_offset, symbol_count, optional_size, _ = struct.unpack_from(
        "<HHIIIHH", data, 0)
    require(machine == 0x8664 and optional_size == 0, f"invalid AMD64 COFF object: {path}")
    sections: list[str] = []
    offset = 20
    for _ in range(section_count):
        require(offset + 40 <= len(data), f"truncated section table: {path}")
        sections.append(data[offset:offset + 8].split(b"\0", 1)[0].decode("ascii", errors="replace"))
        offset += 40
    require(symbol_offset + symbol_count * 18 + 4 <= len(data), f"truncated symbol table: {path}")
    string_offset = symbol_offset + symbol_count * 18
    string_size = struct.unpack_from("<I", data, string_offset)[0]
    require(string_offset + string_size <= len(data), f"truncated string table: {path}")
    symbols: list[str] = []
    index = 0
    while index < symbol_count:
        entry = symbol_offset + index * 18
        zeroes, name_offset = struct.unpack_from("<II", data, entry)
        if zeroes == 0 and name_offset >= 4 and name_offset < string_size:
            begin = string_offset + name_offset
            end = data.find(b"\0", begin, string_offset + string_size)
            if end < 0:
                end = string_offset + string_size
            name = data[begin:end].decode("utf-8", errors="replace")
        else:
            name = data[entry:entry + 8].split(b"\0", 1)[0].decode("ascii", errors="replace")
        if name:
            symbols.append(name)
        auxiliary = data[entry + 17]
        index += 1 + auxiliary
    require(".text" in sections, f"COFF object has no .text: {path}")
    return {"machine": "AMD64", "sections": sections, "symbols": sorted(set(symbols)),
            "sha256": digest(path), "bytes": len(data)}


def link(tool: Path, obj: Path, output: Path, entry: str,
         import_library: Path | None = None) -> None:
    command = [str(tool), "--link", str(obj)]
    if import_library is not None:
        command.extend(["--lib", str(import_library)])
    command.extend(["--entry", entry, "-o", str(output)])
    checked(command, f"NSLink link of {obj.name}")
    require(output.is_file() and output.stat().st_size > 0, f"missing linked image: {output}")


def execute(image: Path, expected: int) -> None:
    result = run([str(image)], 30)
    require(result.returncode == expected,
            f"{image.name} returned {result.returncode}, expected {expected}")


def parse_project_dump(data: bytes) -> dict[str, object]:
    text = canonical(data).decode("utf-8", errors="strict")
    modern = re.search(r"^name=([^\n]+)$.*^output=([^\n]+)$.*^sources=(\d+)$", text,
                       flags=re.MULTILINE | re.DOTALL)
    if modern:
        return {"name": modern.group(1), "kind": modern.group(2).casefold(),
                "sources": int(modern.group(3))}
    legacy = re.search(r"^project ([^\n]+)$.*^  output: ([^\n]+)$.*^  sources: (\d+)$", text,
                       flags=re.MULTILINE | re.DOTALL)
    require(legacy is not None, "unrecognized project dump")
    return {"name": legacy.group(1), "kind": legacy.group(2).casefold(),
            "sources": int(legacy.group(3))}


def parse_legacy_metadata(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    require(data.startswith(b"NSMETA1\0") and len(data) >= 100, "invalid nsc0 NSMETA")
    major, minor = struct.unpack_from("<HH", data, 8)
    payload_size = struct.unpack_from("<Q", data, 28)[0]
    require(100 + payload_size == len(data), "invalid nsc0 NSMETA payload size")
    position = 100

    def text() -> str:
        nonlocal position
        require(position + 4 <= len(data), "truncated NSMETA string")
        size = struct.unpack_from("<I", data, position)[0]
        position += 4
        require(position + size <= len(data), "truncated NSMETA text")
        value = data[position:position + size].decode("utf-8")
        position += size
        return value

    producer = text()
    module = text()
    target = text()
    return {"format": f"{major}.{minor}", "producer": producer, "name": module,
            "kind": "library", "target": target, "abi": "NativeSharp/2.0"}


def parse_modern_metadata(path: Path) -> dict[str, object]:
    fields: dict[str, str] = {}
    lines = path.read_text(encoding="utf-8").splitlines()
    require(lines and lines[0] == "NSMETA/2", "invalid nsc2 NSMETA")
    for line in lines[1:]:
        if "=" in line:
            key, value = line.split("=", 1)
            fields[key] = value
    return {"format": "2", "producer": fields.get("producer"), "name": fields.get("name"),
            "kind": fields.get("kind", "").casefold(), "target": "x86_64-pc-windows-msvc",
            "abi": fields.get("abi"), "compatibility": fields.get("compatibility")}


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify NativeSharp SH42 functional equivalence.")
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--python", type=Path, required=True)
    parser.add_argument("--nsc0", type=Path, required=True)
    for level in ("o0", "o1", "o2", "o3"):
        parser.add_argument(f"--nsc2-{level}", type=Path, required=True)
    parser.add_argument("--nslink", type=Path, required=True)
    parser.add_argument("--import-library", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    try:
        contract = json.loads(args.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == SCHEMA and contract.get("version") == 1,
                "unsupported SH42 contract")
        root = args.source_root.resolve()
        out = args.output_dir.resolve()
        out.mkdir(parents=True, exist_ok=True)
        candidates = {level: getattr(args, f"nsc2_{level.lower()}").resolve()
                      for level in contract["optimizations"]}
        require(args.nsc0.is_file() and all(path.is_file() for path in candidates.values()),
                "missing nsc0 or nsc2 image")

        diagnostics: list[dict[str, object]] = []
        missing = out / "missing-source.ns"
        diagnostic_commands = [["--definitely-unknown"], ["--dump-tokens", str(missing)]]
        for arguments in diagnostic_commands:
            oracle = run([str(args.nsc0), *arguments])
            for profile, candidate in candidates.items():
                actual = run([str(candidate), *arguments])
                require((actual.returncode, canonical(actual.stdout), canonical(actual.stderr)) ==
                        (oracle.returncode, canonical(oracle.stdout), canonical(oracle.stderr)),
                        f"{profile} diagnostic differs for {' '.join(arguments)}")
            diagnostics.append({"arguments": arguments, "exit_code": oracle.returncode,
                                "stderr_sha256": hashlib.sha256(canonical(oracle.stderr)).hexdigest()})

        bootstrap = root / "selfhost/bootstrap"
        frontend_report = call_verifier(args.python, bootstrap / "verify_frontend.py", [
            "--source-root", str(root), "--nsc0", str(args.nsc0),
            "--nsc-next-o0", str(candidates["O0"]), "--nsc-next-o2", str(candidates["O2"]),
            "--contract", str(root / contract["frontend_contract"]),
            "--output-dir", str(out / "frontend")], out / "frontend-report.json")
        semantic_report = call_verifier(args.python, bootstrap / "verify_semantics.py", [
            "--source-root", str(root), "--nsc0", str(args.nsc0),
            "--nsc-next-o0", str(candidates["O0"]), "--nsc-next-o2", str(candidates["O2"]),
            "--contract", str(root / contract["semantic_contract"])], out / "semantic-report.json")
        ir_arguments = ["--source-root", str(root), "--nsc0", str(args.nsc0)]
        for profile, candidate in candidates.items():
            ir_arguments.extend([f"--nsc-next-{profile.lower()}", str(candidate)])
        ir_arguments.extend(["--contract", str(root / contract["ir_contract"])])
        ir_report = call_verifier(args.python, bootstrap / "verify_ir.py", ir_arguments,
                                  out / "ir-report.json")

        object_records: list[dict[str, object]] = []
        executions = 0
        for profile, candidate in candidates.items():
            for relative in contract["object_sources"]:
                source = root / relative
                expected = int(re.search(r"(\d+)\.ns$", source.name).group(1))
                stem = f"{source.stem}-{profile.lower()}"
                legacy_obj = out / f"{stem}-nsc0.obj"
                modern_obj = out / f"{stem}-nsc2.obj"
                checked([str(args.nsc0), "--emit-obj", str(source), f"-{profile}", "-o", str(legacy_obj)],
                        f"nsc0 {profile} object")
                checked([str(candidate), "--emit-object", str(source), f"-{profile}", "-o", str(modern_obj)],
                        f"nsc2 {profile} object")
                summaries = {"nsc0": coff_summary(legacy_obj), "nsc2": coff_summary(modern_obj)}
                require("Main" in summaries["nsc0"]["symbols"], "nsc0 object misses Main")
                require("mainCRTStartup" in summaries["nsc2"]["symbols"],
                        "nsc2 object misses mainCRTStartup")
                for route, tool in (("nslink", args.nslink),):
                    for producer, obj, entry, imports in (
                            ("nsc0", legacy_obj, "Main", None),
                            ("nsc2", modern_obj, "mainCRTStartup", args.import_library)):
                        image = out / f"{stem}-{producer}-{route}.exe"
                        link(tool, obj, image, entry, imports)
                        execute(image, expected)
                        executions += 1
                object_records.append({"profile": profile, "source": relative,
                                       "expected_exit_code": expected, "coff": summaries})

        project = root / contract["project"]
        oracle_dump = checked([str(args.nsc0), "--dump-project", str(project)], "nsc0 project dump")
        oracle_model = parse_project_dump(oracle_dump.stdout)
        require(oracle_model == {"name": contract["project_name"],
                                "kind": contract["project_kind"].casefold(),
                                "sources": contract["source_count"]}, "nsc0 project model changed")
        project_records: list[dict[str, object]] = []
        product_plans = 0
        for profile, candidate in candidates.items():
            modern_dump = checked([str(candidate), "--dump-project", str(project)],
                                  f"nsc2 {profile} project dump")
            require(parse_project_dump(modern_dump.stdout) == oracle_model,
                    f"{profile} project model differs from nsc0")
            legacy_exe = out / f"project-{profile.lower()}-nsc0.exe"
            checked([str(args.nsc0), "--emit-project-oop-exe", str(project), f"-{profile}",
                     "-g0", "-o", str(legacy_exe)], f"nsc0 {profile} project product", 600)
            execute(legacy_exe, contract["expected_exit_code"])
            modern_obj = out / f"project-{profile.lower()}-nsc2.obj"
            checked([str(candidate), "--emit-project-object", str(project), f"-{profile}",
                     "-o", str(modern_obj)], f"nsc2 {profile} project object", 600)
            summary = coff_summary(modern_obj)
            require("mainCRTStartup" in summary["symbols"], f"{profile} project object misses entry")
            for route, tool in (("nslink", args.nslink),):
                image = out / f"project-{profile.lower()}-nsc2-{route}.exe"
                link(tool, modern_obj, image, "mainCRTStartup", args.import_library)
                execute(image, contract["expected_exit_code"])
                plan = out / f"product-{profile.lower()}-{route}.plan"
                checked([str(candidate), "--emit-product-plan", str(project), profile, route, str(plan)],
                        f"nsc2 {profile}/{route} product plan")
                plan_text = plan.read_text(encoding="utf-8")
                require("kind=Exe\n" in plan_text and "product=FunctionalEquivalence.exe\n" in plan_text,
                        f"invalid {profile}/{route} product plan")
                product_plans += 1
                executions += 1
            project_records.append({"profile": profile, "nsc0_product_sha256": digest(legacy_exe),
                                    "nsc2_object": summary})

        library_project = root / contract["library_project"]
        legacy_meta = out / "nsc0.nsmeta"
        modern_meta = out / "nsc2.nsmeta"
        checked([str(args.nsc0), "--emit-project-library-metadata", str(library_project),
                 "-o", str(legacy_meta)], "nsc0 metadata", 600)
        checked([str(args.nsc0), "--verify-library-metadata", str(legacy_meta)], "nsc0 metadata read")
        checked([str(candidates["O0"]), "--emit-nsmeta", str(library_project), str(modern_meta)],
                "nsc2 metadata")
        checked([str(candidates["O0"]), "--verify-nsmeta", str(modern_meta)], "nsc2 metadata read")
        legacy_metadata = parse_legacy_metadata(legacy_meta)
        modern_metadata = parse_modern_metadata(modern_meta)
        require(legacy_metadata["name"] == modern_metadata["name"] and
                legacy_metadata["kind"] == modern_metadata["kind"] and
                legacy_metadata["target"] == modern_metadata["target"] and
                legacy_metadata["abi"] == modern_metadata["abi"],
                "metadata semantic headers differ")

        report = {
            "schema": REPORT_SCHEMA, "version": 1, "status": "passed", "gate": "SH42", "tanda": 44,
            "layers": contract["required_layers"], "normalization_policy": contract["normalization_policy"],
            "diagnostics": diagnostics,
            "frontend": frontend_report, "semantics": semantic_report, "ir": ir_report,
            "objects": object_records, "object_cases": len(object_records),
            "projects": project_records, "project_cases": len(project_records),
            "products": {"nsc0_executables": len(project_records), "nsc2_plans": product_plans},
            "metadata": {"nsc0": legacy_metadata, "nsc2": modern_metadata,
                         "semantic_header_equivalent": True,
                         "hashes": {"nsc0": digest(legacy_meta), "nsc2": digest(modern_meta)}},
            "executions": executions, "skipped": 0, "problems": [],
            "binary_sha256": {"nsc0": digest(args.nsc0),
                              **{f"nsc2_{key.lower()}": digest(value) for key, value in candidates.items()}},
        }
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                               encoding="utf-8", newline="\n")
    except (OSError, UnicodeError, ValueError, json.JSONDecodeError,
            subprocess.SubprocessError, VerificationError) as error:
        print(f"verify_functional_equivalence: error: {error}", file=sys.stderr)
        return 1
    print("SH42 passed: nsc0/nsc2 diagnostics, frontend, semantics, IR, objects, metadata, products and projects")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
