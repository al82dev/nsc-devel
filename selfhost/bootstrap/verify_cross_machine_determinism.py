#!/usr/bin/env python3
"""Build and compare SH44 deterministic artifacts, then verify a peer attestation."""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import time
from pathlib import Path

try:
    from .profile_selection import selected_profiles
except ImportError:
    from profile_selection import selected_profiles

CONTRACT_SCHEMA = "nativesharp-selfhost-cross-machine-determinism-contract"
REPORT_SCHEMA = "nativesharp-selfhost-sh44-report"
ATTESTATION_SCHEMA = "nativesharp-selfhost-sh44-machine-attestation"


class VerificationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical_digest(value: object) -> str:
    encoded = json.dumps(value, ensure_ascii=False, sort_keys=True,
                         separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def run(command: list[str], environment: dict[str, str] | None = None) -> tuple[subprocess.CompletedProcess[str], float]:
    started = time.perf_counter()
    result = subprocess.run(command, env=environment, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            check=False)
    return result, time.perf_counter() - started


def ordered_files(source_root: Path, roots: list[str]) -> list[Path]:
    files: list[Path] = []
    for relative in roots:
        directory = source_root / relative
        require(directory.is_dir(), f"missing source root: {directory}")
        files.extend(path for path in directory.rglob("*") if path.is_file())
    return sorted(set(files), key=lambda path: path.relative_to(source_root).as_posix())


def reorder(paths: list[Path], mode: str) -> list[Path]:
    if mode == "ascending":
        return paths
    if mode == "descending":
        return list(reversed(paths))
    if mode == "even-odd":
        return paths[::2] + paths[1::2]
    if mode == "odd-even":
        return paths[1::2] + paths[::2]
    raise VerificationError(f"unsupported creation order: {mode}")


def source_manifest(source_root: Path, files: list[Path]) -> tuple[str, list[dict[str, object]]]:
    records: list[dict[str, object]] = []
    for path in files:
        records.append({
            "path": path.relative_to(source_root).as_posix(),
            "size": path.stat().st_size,
            "sha256": digest(path),
        })
    return canonical_digest(records), records


def stage_sources(source_root: Path, files: list[Path], target: Path,
                  creation_order: str) -> list[str]:
    require(not target.exists(), f"staging root is not clean: {target}")
    target.mkdir(parents=True)
    created: list[str] = []
    for source in reorder(files, creation_order):
        relative = source.relative_to(source_root)
        destination = target / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
        created.append(relative.as_posix())
    return created


def link(tool: Path, obj: Path, libraries: list[Path], output: Path) -> float:
    command = [str(tool), "--link", str(obj)]
    for library in libraries:
        command.extend(["--lib", str(library)])
    command.extend(["--entry", "mainCRTStartup", "-o", str(output)])
    result, elapsed = run(command)
    require(result.returncode == 0,
            f"NSLink failed ({result.returncode}): {result.stdout}{result.stderr}")
    require(output.is_file() and output.stat().st_size > 0, f"missing linked image: {output}")
    return elapsed


def machine_identity() -> dict[str, str]:
    fields = {
        "node": platform.node(),
        "platform": platform.platform(),
        "processor": platform.processor(),
        "architecture": platform.machine(),
    }
    fields["fingerprint"] = canonical_digest(fields)
    return fields


def verify_image(image: Path, expected_identity: str, project: Path,
                 environment: dict[str, str]) -> dict[str, object]:
    version, version_seconds = run([str(image), "--version"], environment)
    require(version.returncode == 0 and version.stdout.strip() == expected_identity,
            f"identity mismatch for {image}: {version.stdout}{version.stderr}")
    self_test, self_test_seconds = run([str(image), "--self-test-basic"], environment)
    require(self_test.returncode == 42, f"self-test failed for {image}")
    checked, project_seconds = run([str(image), "--check-project", str(project)], environment)
    require(checked.returncode == 0,
            f"project check failed for {image}: {checked.stdout}{checked.stderr}")
    return {
        "version_seconds": round(version_seconds, 6),
        "self_test_seconds": round(self_test_seconds, 6),
        "project_seconds": round(project_seconds, 6),
    }


def execute_profile(profile: str, candidate: Path, project: Path, case_root: Path,
                    environment: dict[str, str], nslink: Path,
                    libraries: list[Path], expected_identity: str) -> tuple[str, dict[str, object], dict[str, object]]:
    profile_dir = case_root / profile.lower()
    profile_dir.mkdir(parents=True)
    obj = profile_dir / "nsc3.obj"
    emitted, emit_seconds = run([
        str(candidate), "--emit-bootstrap-object", str(project),
        f"-{profile}", "-o", str(obj),
    ], environment)
    require(emitted.returncode == 0,
            f"{case_root.name}/{profile} emission failed: {emitted.stdout}{emitted.stderr}")
    require(obj.is_file() and obj.stat().st_size > 0, f"missing object: {obj}")
    images: dict[str, str] = {}
    link_times: dict[str, float] = {}
    validation: dict[str, object] = {}
    route = "nslink"
    image = profile_dir / "nsc3-nslink.exe"
    link_times[route] = round(link(nslink, obj, libraries, image), 6)
    images[route] = digest(image)
    validation[route] = verify_image(image, expected_identity, project, environment)
    return profile, {"object": digest(obj), "images": images}, {
        "emit_seconds": round(emit_seconds, 6),
        "link_seconds": link_times,
        "validation": validation,
    }


def compare_peer(local: dict[str, object], peer: dict[str, object]) -> None:
    require(peer.get("schema") == ATTESTATION_SCHEMA and peer.get("version") == 1,
            "unsupported peer attestation")
    local_machine = local["machine"]
    peer_machine = peer.get("machine", {})
    require(isinstance(peer_machine, dict), "peer machine identity is missing")
    require(peer_machine.get("fingerprint") != local_machine.get("fingerprint"),
            "peer attestation belongs to the same machine")
    for field in ("contract_sha256", "source_sha256", "input_sha256", "artifact_sha256"):
        require(peer.get(field) == local.get(field), f"peer {field} differs")
    require(peer.get("local_matrix_passed") is True, "peer local matrix did not pass")
    require(peer.get("normalization_applied") == [], "peer applied unauthorized normalization")


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify NativeSharp SH44 determinism.")
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    for level in ("o0", "o1", "o2", "o3"):
        parser.add_argument(f"--nsc2-{level}", type=Path)
    parser.add_argument("--nslink", type=Path, required=True)
    parser.add_argument("--runtime-library", type=Path, required=True)
    parser.add_argument("--llvm-library", type=Path, required=True)
    parser.add_argument("--import-library", action="append", type=Path, default=[])
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--attestation", type=Path, required=True)
    parser.add_argument("--peer-attestation", type=Path)
    parser.add_argument("--require-peer", action="store_true")
    args = parser.parse_args()
    try:
        contract = json.loads(args.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == CONTRACT_SCHEMA and contract.get("version") == 1,
                "unsupported SH44 contract")
        required_machines = contract.get("required_machines")
        require(required_machines in (1, 2), "SH44 requires one or two machines")
        require(contract.get("normalization") == [], "SH44 forbids artifact normalization")
        root = args.source_root.resolve()
        output = args.output_dir.resolve()
        require(not output.exists(), f"SH44 output root is not clean: {output}")
        output.mkdir(parents=True)
        profiles = selected_profiles(contract["optimizations"])
        candidates: dict[str, Path] = {}
        for profile in profiles:
            candidate = getattr(args, f"nsc2_{profile.lower()}")
            require(candidate is not None, f"missing selected nsc2 image argument: {profile}")
            candidates[profile] = candidate.resolve()
        libraries = [args.runtime_library.resolve(), args.llvm_library.resolve(),
                     *(item.resolve() for item in args.import_library)]
        inputs = [*candidates.values(), args.nslink.resolve(), *libraries]
        require(all(path.is_file() for path in inputs), "one or more SH44 inputs are missing")
        files = ordered_files(root, contract["source_roots"])
        source_sha256, manifest = source_manifest(root, files)
        (output / "source-manifest.json").write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8", newline="\n")
        snapshot_root = output / "snapshot" / "root"
        stage_sources(root, files, snapshot_root, "ascending")
        snapshot_files = ordered_files(snapshot_root, contract["source_roots"])
        snapshot_sha256, snapshot_manifest = source_manifest(snapshot_root, snapshot_files)
        require(snapshot_sha256 == source_sha256 and snapshot_manifest == manifest,
                "source tree changed while SH44 snapshot was created")

        records: list[dict[str, object]] = []
        artifact_hashes: dict[str, dict[str, object]] = {}
        reference: dict[str, dict[str, str]] = {}
        for variant in contract["variants"]:
            case_root = output / "artifacts" / variant["name"]
            staged_root = output / "staged" / variant["path"] / "root"
            creation = stage_sources(snapshot_root, snapshot_files, staged_root,
                                     variant["creation_order"])
            case_root.mkdir(parents=True)
            project = staged_root / contract["compiler_project"]
            environment = os.environ.copy()
            environment.update({
                "LANG": variant["locale"], "LC_ALL": variant["locale"],
                "TZ": variant["timezone"], "NSC_SH44_VARIANT": variant["name"],
            })
            profile_hashes: dict[str, object] = {}
            timings: dict[str, object] = {}
            completed: list[tuple[str, dict[str, object], dict[str, object]]] = []
            with ThreadPoolExecutor(max_workers=variant["workers"]) as executor:
                pending = [executor.submit(
                    execute_profile, profile, candidates[profile], project, case_root,
                    environment, args.nslink.resolve(), libraries,
                    contract["expected_identity"]) for profile in profiles]
                for future in as_completed(pending):
                    completed.append(future.result())
            for profile, observed, observed_timings in sorted(completed):
                images = observed["images"]
                if profile not in reference:
                    reference[profile] = {"object": observed["object"], **images}
                else:
                    require(observed["object"] == reference[profile]["object"],
                            f"{variant['name']}/{profile} object differs")
                    for route in contract["linkers"]:
                        require(images[route] == reference[profile][route],
                                f"{variant['name']}/{profile}/{route} image differs")
                profile_hashes[profile] = observed
                timings[profile] = observed_timings
            artifact_hashes[variant["name"]] = profile_hashes
            records.append({
                **variant,
                "absolute_path_length": len(str(staged_root.resolve())),
                "created_files": len(creation),
                "creation_manifest_sha256": canonical_digest(creation),
                "timings_seconds": timings,
            })

        final_files = ordered_files(root, contract["source_roots"])
        final_source_sha256, final_manifest = source_manifest(root, final_files)
        require(final_source_sha256 == source_sha256 and final_manifest == manifest,
                "source tree changed during SH44")

        input_sha256 = {
            "nsc2": {key: digest(value) for key, value in sorted(candidates.items())},
            "nslink": digest(args.nslink.resolve()),
            "libraries": {path.name: digest(path) for path in libraries},
        }
        attestation: dict[str, object] = {
            "schema": ATTESTATION_SCHEMA, "version": 1, "gate": "SH44", "tanda": 46,
            "machine": machine_identity(), "contract_sha256": digest(args.contract),
            "source_sha256": source_sha256, "source_files": len(files),
            "snapshot_source_sha256": snapshot_sha256,
            "input_sha256": input_sha256, "artifact_sha256": reference,
            "local_matrix_passed": True, "normalization_applied": [],
            "variants": len(records), "profiles": len(profiles),
            "linkers": len(contract["linkers"]),
        }
        args.attestation.parent.mkdir(parents=True, exist_ok=True)
        args.attestation.write_text(
            json.dumps(attestation, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8", newline="\n")
        peer: dict[str, object] | None = None
        if args.peer_attestation:
            require(args.peer_attestation.is_file(), "peer attestation does not exist")
            peer = json.loads(args.peer_attestation.read_text(encoding="utf-8"))
            compare_peer(attestation, peer)
        if args.require_peer or required_machines == 2:
            require(peer is not None, "SH44 closure requires an attestation from a second machine")
        requirement_met = peer is not None or required_machines == 1
        report = {
            "schema": REPORT_SCHEMA, "version": 1, "gate": "SH44", "tanda": 46,
            "status": "passed" if requirement_met else "local-passed",
            "machines_verified": 2 if peer is not None else 1,
            "required_machines": required_machines, "local_matrix_passed": True,
            "cross_machine_verified": peer is not None,
            "machine": attestation["machine"],
            "peer_machine": peer.get("machine") if peer else None,
            "source_files": len(files), "source_sha256": source_sha256,
            "variants": records, "variant_count": len(records),
            "profiles": profiles, "linkers": contract["linkers"],
            "object_comparisons": len(profiles) * (len(records) - 1),
            "image_comparisons": len(profiles) * len(contract["linkers"]) *
                                 (len(records) - 1),
            "artifact_sha256": reference, "normalization_applied": [], "skipped": 0,
            "problems": [] if requirement_met else
                ["SH44 requires a matching attestation from a second Windows x64 machine"],
        }
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(
            json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8", newline="\n")
    except (OSError, UnicodeError, ValueError, json.JSONDecodeError,
            subprocess.SubprocessError, VerificationError) as error:
        print(f"verify_cross_machine_determinism: error: {error}", file=sys.stderr)
        return 1
    if peer is None and required_machines == 1:
        print("SH44 passed: deterministic artifacts verified across local environment variants")
    elif peer is None:
        print("SH44 local matrix passed; a distinct-machine attestation is still required")
    else:
        print("SH44 passed: deterministic artifacts verified on two distinct machines")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
