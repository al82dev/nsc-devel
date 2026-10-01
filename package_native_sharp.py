"""Create a versioned, flat-root NativeSharp distribution archive."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime
import os
from pathlib import Path, PureWindowsPath
import re
import subprocess
import sys


DEFAULT_PREFIX = Path(r"C:\native-sharp")
DEFAULT_SEVEN_ZIP = Path(r"A:\7z\7z.exe")
DEFAULT_OUTPUT = Path("A:\\")


@dataclass(frozen=True)
class BuildIdentity:
    version: str
    build_hash: str


def resolved(path: str | Path) -> Path:
    return Path(path).expanduser().resolve()


def safe_filename_token(value: str, label: str) -> str:
    token = re.sub(r"[^A-Za-z0-9._-]+", "-", value.strip()).strip(".-_")
    if not token:
        raise ValueError(f"{label} is empty or cannot be used in a file name")
    return token


def parse_identity(output: str) -> BuildIdentity:
    version_patterns = (
        r"(?im)^NativeSharp Compiler\s+([^\s]+)",
        r"(?im)\bNativeSharp\s+([0-9]+(?:\.[0-9]+)+(?:[-._][A-Za-z0-9.-]+)?)",
        r"(?im)^nsc\s+([0-9]+(?:\.[0-9]+)+)",
    )
    version = next(
        (match.group(1) for pattern in version_patterns if (match := re.search(pattern, output))),
        None,
    )
    hash_match = re.search(r"(?im)^(?:Build hash:|build)\s+([0-9a-f]{7,64})\s*$", output)
    if version is None:
        raise ValueError("nsc --version did not report a NativeSharp version")
    if hash_match is None:
        raise ValueError("nsc --version did not report a build hash")
    return BuildIdentity(
        safe_filename_token(version, "version"),
        safe_filename_token(hash_match.group(1).lower(), "build hash"),
    )


def read_identity(prefix: Path) -> BuildIdentity:
    compiler = prefix / "bin" / "nsc.exe"
    if not compiler.is_file():
        raise FileNotFoundError(f"NativeSharp compiler not found: {compiler}")
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    result = subprocess.run(
        [str(compiler), "--version", "--no-error-log"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=15,
        creationflags=flags,
        check=False,
    )
    output = (result.stdout + "\n" + result.stderr).strip()
    if result.returncode != 0:
        raise RuntimeError(f"nsc --version exited with code {result.returncode}:\n{output}")
    return parse_identity(output)


def archive_name(identity: BuildIdentity, timestamp: str) -> str:
    stamp = safe_filename_token(timestamp, "timestamp")
    return f"native-sharp-{identity.version}-{identity.build_hash}-{stamp}.7z"


def source_entries(prefix: Path) -> list[Path]:
    entries = sorted(prefix.iterdir(), key=lambda path: path.name.casefold())
    if not entries:
        raise ValueError(f"installation directory is empty: {prefix}")
    return entries


def command_line(arguments: list[str]) -> str:
    return subprocess.list2cmdline(arguments)


def list_archive_paths(seven_zip: Path, archive: Path) -> list[PureWindowsPath]:
    result = subprocess.run(
        [str(seven_zip), "l", "-slt", str(archive)],
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(f"7-Zip could not inspect the archive:\n{result.stdout}{result.stderr}")

    paths: list[PureWindowsPath] = []
    archive_resolved = os.path.normcase(str(archive.resolve()))
    for line in result.stdout.splitlines():
        if not line.startswith("Path = "):
            continue
        value = line[7:].strip()
        if not value or os.path.normcase(value) == archive_resolved:
            continue
        paths.append(PureWindowsPath(value))
    if not paths:
        raise RuntimeError("7-Zip reported no files in the generated archive")
    return paths


def verify_flat_root(paths: list[PureWindowsPath], entries: list[Path]) -> None:
    top_levels: set[str] = set()
    for archived in paths:
        if archived.is_absolute() or ".." in archived.parts:
            raise RuntimeError(f"unsafe path in archive: {archived}")
        if archived.parts:
            top_levels.add(archived.parts[0].casefold())

    expected = {entry.name.casefold() for entry in entries}
    missing = sorted(expected - top_levels)
    if missing:
        raise RuntimeError("archive root is missing: " + ", ".join(missing))


def create_archive(
    prefix: Path,
    seven_zip: Path,
    output_dir: Path,
    compression: int,
    timestamp: str,
    dry_run: bool,
) -> Path:
    if prefix == Path(prefix.anchor):
        raise ValueError("the installation directory cannot be a drive root")
    if not prefix.is_dir():
        raise FileNotFoundError(f"installation directory not found: {prefix}")
    if not seven_zip.is_file():
        raise FileNotFoundError(f"7-Zip executable not found: {seven_zip}")
    if not 0 <= compression <= 9:
        raise ValueError("compression must be between 0 and 9")

    identity = read_identity(prefix)
    entries = source_entries(prefix)
    final_archive = output_dir / archive_name(identity, timestamp)
    partial_archive = final_archive.with_name(final_archive.name + ".partial")
    arguments = [
        str(seven_zip),
        "a",
        "-t7z",
        f"-mx={compression}",
        "-mmt=on",
        "-y",
        str(partial_archive),
        *(entry.name for entry in entries),
    ]

    print(f"Source     : {prefix}")
    print(f"Version    : {identity.version}")
    print(f"Build hash : {identity.build_hash}")
    print(f"Archive    : {final_archive}")
    print(f"Root items : {len(entries)}")
    print(f"> {command_line(arguments)}")
    if dry_run:
        print("Dry run: no archive was created.")
        return final_archive

    output_dir.mkdir(parents=True, exist_ok=True)
    if final_archive.exists() or partial_archive.exists():
        raise FileExistsError(f"output already exists: {final_archive}")

    try:
        process = subprocess.Popen(
            arguments,
            cwd=prefix,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            errors="replace",
        )
        assert process.stdout is not None
        for line in process.stdout:
            print(line, end="")
        return_code = process.wait()
        if return_code != 0:
            raise RuntimeError(f"7-Zip exited with code {return_code}")
        archived_paths = list_archive_paths(seven_zip, partial_archive)
        verify_flat_root(archived_paths, entries)
        partial_archive.replace(final_archive)
    except BaseException:
        if partial_archive.is_file():
            partial_archive.unlink()
        raise

    print(f"Package created: {final_archive}")
    return final_archive


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", default=str(DEFAULT_PREFIX), help="installed NativeSharp root")
    parser.add_argument("--seven-zip", default=str(DEFAULT_SEVEN_ZIP), help="path to 7z.exe")
    parser.add_argument("--output-dir", default=str(DEFAULT_OUTPUT), help="archive destination")
    parser.add_argument("--compression", type=int, default=9, help="7-Zip compression level (0-9)")
    parser.add_argument(
        "--timestamp",
        help="timestamp used in the file name (default: current local time)",
    )
    parser.add_argument("--dry-run", action="store_true", help="validate and print without archiving")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    timestamp = arguments.timestamp or datetime.now().astimezone().strftime("%Y%m%d-%H%M%S")
    try:
        create_archive(
            resolved(arguments.prefix),
            resolved(arguments.seven_zip),
            resolved(arguments.output_dir),
            arguments.compression,
            timestamp,
            arguments.dry_run,
        )
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
