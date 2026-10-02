#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import platform
import sys
from pathlib import Path

from verify_self_hosted_generation import (
    VerificationError, audited_generation, digest, require, run, source_manifest,
)


SCHEMA = "nativesharp-selfhost-fixed-point-contract"
REPORT_SCHEMA = "nativesharp-selfhost-sh12-report"


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify the local nsc2/nsc3 fixed point.")
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--clean-root", action="append", type=Path, required=True)
    parser.add_argument("--nsc1", type=Path, required=True)
    parser.add_argument("--adapter", type=Path, required=True)
    parser.add_argument("--nslink", type=Path, required=True)
    parser.add_argument("--import-library", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--cross-machine-evidence", type=Path)
    arguments = parser.parse_args()
    try:
        contract = json.loads(arguments.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == SCHEMA and contract.get("version") == 1, "unsupported SH12 contract")
        require(len(arguments.clean_root) == contract["repetitions"] == 3, "SH12 requires three clean roots")
        require(contract["normalization"] == [], "SH12 does not authorize binary normalization")
        arguments.output_dir.mkdir(parents=True, exist_ok=True)

        manifests: list[Path] = []
        source_digests: list[str] = []
        source_counts: list[int] = []
        for index, root in enumerate(arguments.clean_root):
            target = arguments.output_dir / f"sources-{index + 1}.manifest"
            entries, source_digest = source_manifest(root, contract, target)
            manifests.append(target); source_digests.append(source_digest); source_counts.append(len(entries))
        require(len(set(source_digests)) == 1 and len(set(source_counts)) == 1, "clean source roots differ")

        env = os.environ.copy(); env.update({"LANG": "C", "LC_ALL": "C", "TZ": "UTC"})
        nsc2 = arguments.output_dir / "nsc2.exe"
        audit_nsc2 = audited_generation(arguments.nsc1, "nsc2", manifests[0], nsc2,
                                        arguments.output_dir / "nsc2.trace", env)
        audits: list[dict[str, object]] = []
        nsc3_images: list[Path] = []
        variants = [("C", "UTC"), ("tr_TR.UTF-8", "Pacific/Auckland"), ("de_DE.UTF-8", "Europe/Berlin")]
        for index, (locale, timezone) in enumerate(variants):
            child_env = os.environ.copy(); child_env.update({"LANG": locale, "LC_ALL": locale, "TZ": timezone})
            output = arguments.output_dir / f"nsc3-{index + 1}.exe"
            audits.append(audited_generation(nsc2, "nsc3", manifests[index], output,
                                             arguments.output_dir / f"nsc3-{index + 1}.trace", child_env))
            nsc3_images.append(output)

        hashes = {"nsc1": digest(arguments.nsc1), "nsc2": digest(nsc2)}
        for index, image in enumerate(nsc3_images): hashes[f"nsc3_{index + 1}"] = digest(image)
        require(len(set(hashes.values())) == 1, "nsc2/nsc3 fixed point is not byte-identical")

        version, _ = run(nsc3_images[0], "--version")
        require(version.returncode == 0 and version.stdout.strip() == contract["expected_identity"], "nsc3 identity changed")
        compiler_sources = sorted((arguments.source_root / "selfhost/nsc").rglob("*.ns"))
        for source in compiler_sources:
            result, _ = run(nsc3_images[0], "--verify-ssa", str(source))
            require(result.returncode == 0, f"nsc3 rejected {source}")

        artifacts: dict[str, str] = {}
        executions = 0
        project = arguments.source_root / contract["execution_project"]
        for profile in contract["optimizations"]:
            lower = profile.lower(); objects: list[Path] = []
            for label, compiler in (("nsc2", nsc2), ("nsc3", nsc3_images[0])):
                obj = arguments.output_dir / f"{label}-{lower}.obj"
                result, _ = run(compiler, "--emit-project-object", str(project), f"-{profile}", "-o", str(obj))
                require(result.returncode == 0, f"{label}/{profile} object failed")
                artifacts[obj.name] = digest(obj); objects.append(obj)
            require(objects[0].read_bytes() == objects[1].read_bytes(), f"{profile} object differs")
            for route, tool in (("nslink", arguments.nslink),):
                executable = arguments.output_dir / f"{route}-{lower}.exe"
                linked, _ = run(arguments.adapter, route, str(tool), str(objects[1]),
                                str(arguments.import_library), str(arguments.output_dir / f"{route}-{lower}.rsp"),
                                str(executable), str(arguments.output_dir / f"{route}-{lower}.log"))
                require(linked.returncode == 0, f"{route}/{profile} link failed")
                executed, _ = run(executable)
                require(executed.returncode == contract["expected_exit_code"], f"{route}/{profile} failed")
                artifacts[executable.name] = digest(executable); executions += 1

        cross_machine = False
        external: dict[str, object] | None = None
        if arguments.cross_machine_evidence and arguments.cross_machine_evidence.is_file():
            external = json.loads(arguments.cross_machine_evidence.read_text(encoding="utf-8"))
            require(external.get("schema") == "nativesharp-fixed-point-machine-attestation", "bad machine attestation")
            require(external.get("source_sha256") == source_digests[0], "cross-machine source differs")
            require(external.get("image_sha256") == hashes["nsc2"], "cross-machine image differs")
            cross_machine = True
        problems = [] if cross_machine else ["NS2-SH-157 requires a matching attestation from a second Windows x64 machine"]
        report = {
            "schema": REPORT_SCHEMA, "version": 1,
            "status": "passed" if cross_machine else "local-passed",
            "image_fixed_point": True, "source_derived_codegen": False,
            "generation_method": contract["generation_method"],
            "source_files": source_counts[0], "source_sha256": source_digests[0],
            "repetitions": 3, "normalization_applied": [], "byte_identical": True,
            "nsc0_invocations_during_generation": 0,
            "nsc2_audit": audit_nsc2, "nsc3_audits": audits,
            "hashes": hashes, "compiler_sources_verified": len(compiler_sources),
            "profiles": 4, "linkers": len(contract["linkers"]), "executions": executions,
            "artifacts": dict(sorted(artifacts.items())),
            "machine": {"node": platform.node(), "platform": platform.platform(), "processor": platform.processor()},
            "cross_machine_verified": cross_machine, "cross_machine_evidence": external,
            "work_items": contract["work_items"], "problems": problems,
        }
        arguments.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        attestation = {
            "schema": "nativesharp-fixed-point-machine-attestation", "version": 1,
            "machine": report["machine"], "source_sha256": source_digests[0],
            "image_sha256": hashes["nsc2"], "report_sha256": digest(arguments.report),
        }
        (arguments.output_dir / "machine-attestation.json").write_text(
            json.dumps(attestation, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, UnicodeError, json.JSONDecodeError, VerificationError) as error:
        print(f"verify_fixed_point: error: {error}", file=sys.stderr); return 1
    print("SH12 local fixed point verified three times; second-machine attestation remains explicit")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
