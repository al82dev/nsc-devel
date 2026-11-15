"""Exercise default package lookup without touching the installed NativeSharp toolchain."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(command, cwd, env):
    return subprocess.run([str(item) for item in command], cwd=cwd, env=env,
                          capture_output=True, text=True, errors="replace", timeout=180)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--libraries", required=True, type=Path)
    parser.add_argument("--runtime", required=True, type=Path)
    parser.add_argument("--linker", required=True, type=Path)
    parser.add_argument("--imports", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1] / "cases/libraries/list_findall_effects/Main.ns"
    with tempfile.TemporaryDirectory(prefix="nsc-packages-", dir=args.work_dir) as temporary:
        root = Path(temporary)
        prefix = root / "installation"
        binary = prefix / "bin"
        binary.mkdir(parents=True)
        shutil.copyfile(args.compiler, binary / "nsc.exe")
        env = os.environ.copy()
        for name in ("TEMP", "TMP", "APPDATA", "LOCALAPPDATA"):
            env[name] = str(root)
        env.pop("NS_LIB_ROOT", None)
        env.pop("NSC_CORE_LIBRARY_ROOT", None)
        env["NSC_WINDOWS_RUNTIME_LIBRARY"] = str(args.runtime.resolve())
        env["NSC_LINKER"] = str(args.linker.resolve())
        env["NSC_WINDOWS_RUNTIME_IMPORT_LIBRARY_DIRECTORY"] = str(args.imports.resolve())
        packages = prefix / "packages"
        core = args.libraries / "stdlib/NativeSharp.Core/NativeSharp.Core.nsproj"
        generic = args.libraries / "stdlib/NativeSharp.Core.Collections.Generic/NativeSharp.Core.Collections.Generic.nsproj"
        project = root / "Case.nsproj"
        shutil.copyfile(source, root / "Main.ns")
        project.write_text('<Project><PropertyGroup><Name>PackageProbe</Name><OutputType>Exe</OutputType>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                           '<LibraryReference Include="NativeSharpCore.lib" Metadata="NativeSharpCore.nsmeta" />'
                           '<LibraryReference Include="NativeSharpCoreCollectionsGeneric.lib" '
                           'Metadata="NativeSharpCoreCollectionsGeneric.nsmeta" />'
                           '</ItemGroup></Project>', encoding="utf-8")
        (root / "Platform.ns").write_text(
            'using NativeSharp.Core.IO; using NativeSharp.Core.Threading; '
            'using NativeSharp.Core.Diagnostics; int Main() { Thread.Sleep(0); '
            'return Path.DirectorySeparatorChar() == (char)92 && SystemInfo.PageSize() > 0 ? 0 : 1; }',
            encoding="utf-8")
        platform = root / "Platform.nsproj"
        platform.write_text('<Project><PropertyGroup><Name>PlatformProbe</Name><OutputType>Exe</OutputType>'
                            '</PropertyGroup><ItemGroup><Compile Include="Platform.ns" />'
                            '<LibraryReference Include="NativeSharpCore.lib" Metadata="NativeSharpCore.nsmeta" />'
                            '<LibraryReference Include="NativeSharpCoreIO.lib" Metadata="NativeSharpCoreIO.nsmeta" />'
                            '<LibraryReference Include="NativeSharpCoreThreading.lib" Metadata="NativeSharpCoreThreading.nsmeta" />'
                            '<LibraryReference Include="NativeSharpCoreDiagnostics.lib" Metadata="NativeSharpCoreDiagnostics.nsmeta" />'
                            '</ItemGroup></Project>', encoding="utf-8")
        old_cwd = Path.cwd()
        os.chdir(binary)
        try:
            for level in ("O0", "O1", "O2", "O3"):
                core_dir = packages / "NativeSharp.Core" / "test" / "windows-x64" / level
                generic_dir = packages / "NativeSharp.Core.Collections.Generic" / "test" / "windows-x64" / level
                core_dir.mkdir(parents=True)
                generic_dir.mkdir(parents=True)
                for source_project, target, reference_root in (
                    (core, core_dir / "NativeSharpCore.lib", None),
                    (generic, generic_dir / "NativeSharpCoreCollectionsGeneric.lib", core_dir),
                ):
                    command = ["nsc.exe", "--emit-project-oop-lib", source_project, "-" + level,
                               "-o", target, "--no-artifact-cache", "--no-error-log", "--no-banner"]
                    if reference_root:
                        command += ["--library-reference-root", reference_root]
                    result = run(command, binary, env)
                    if result.returncode:
                        raise RuntimeError(level + " package build: " + result.stdout + result.stderr)
                for package_name, artifact in (("NativeSharp.Core.IO", "NativeSharpCoreIO"),
                                               ("NativeSharp.Core.Threading", "NativeSharpCoreThreading"),
                                               ("NativeSharp.Core.Diagnostics", "NativeSharpCoreDiagnostics")):
                    directory = packages / package_name / "test" / "windows-x64" / level
                    directory.mkdir(parents=True)
                    source_project = args.libraries / "stdlib" / package_name / (package_name + ".nsproj")
                    command = ["nsc.exe", "--emit-project-oop-lib", source_project, "-" + level,
                               "-o", directory / (artifact + ".lib"), "--library-reference-root", core_dir,
                               "--no-artifact-cache", "--no-error-log", "--no-banner"]
                    result = run(command, binary, env)
                    if result.returncode:
                        raise RuntimeError(level + " " + package_name + ": " + result.stdout + result.stderr)
                output = root / (level + ".exe")
                result = run(["nsc.exe", "--emit-project-oop-exe", project, "-" + level,
                              "-o", output, "--no-artifact-cache", "--no-error-log", "--no-banner"], binary, env)
                if result.returncode:
                    raise RuntimeError(level + " consumer build: " + result.stdout + result.stderr)
                result = run([output], root, env)
                if result.returncode:
                    raise RuntimeError(level + " consumer exit " + str(result.returncode))
                platform_output = root / ("platform-" + level + ".exe")
                result = run(["nsc.exe", "--emit-project-oop-exe", platform, "-" + level,
                              "-o", platform_output, "--no-artifact-cache", "--no-error-log", "--no-banner"], binary, env)
                if result.returncode:
                    raise RuntimeError(level + " platform build: " + result.stdout + result.stderr)
                result = run([platform_output], root, env)
                if result.returncode:
                    raise RuntimeError(level + " platform exit " + str(result.returncode))
                print(level + " five external standard packages: PASS", flush=True)

            last_level = packages / "NativeSharp.Core.Threading" / "test" / "windows-x64" / "O3"
            withheld = last_level.with_name("withheld-O3")
            if not last_level.resolve().is_relative_to(root.resolve()) or withheld.exists():
                raise RuntimeError("invalid temporary package paths")
            last_level.rename(withheld)
            try:
                result = run(["nsc.exe", "--check-project-method-bodies", platform, "-O3",
                              "--no-error-log", "--no-banner"], binary, env)
                if result.returncode == 0 or "NativeSharpCoreThreading.lib" not in (result.stdout + result.stderr):
                    raise RuntimeError("missing optimization package must fail: " + result.stdout + result.stderr)
            finally:
                withheld.rename(last_level)

            local_library = root / "NativeSharpCore.lib"
            local_metadata = root / "NativeSharpCore.nsmeta"
            local_library.write_bytes(b"not-an-archive")
            local_metadata.write_bytes(b"not-metadata")
            check = ["nsc.exe", "--check-project-method-bodies", project, "--no-error-log", "--no-banner"]
            result = run(check, binary, env)
            if result.returncode == 0 or str(local_metadata).replace("\\", "/") not in (result.stdout + result.stderr):
                raise RuntimeError("local library must take precedence: " + result.stdout + result.stderr)
            flat = root / "explicit"
            flat.mkdir()
            for package_name, artifact in (("NativeSharp.Core", "NativeSharpCore"),
                                           ("NativeSharp.Core.Collections.Generic", "NativeSharpCoreCollectionsGeneric")):
                folder = packages / package_name / "test" / "windows-x64" / "O0"
                for extension in (".lib", ".nsmeta"):
                    shutil.copyfile(folder / (artifact + extension), flat / (artifact + extension))
            result = run(check + ["--library-reference-root", flat], binary, env)
            if result.returncode:
                raise RuntimeError("explicit root must take precedence: " + result.stdout + result.stderr)
            local_library.unlink()
            local_metadata.unlink()

            duplicate = packages / "NativeSharp.Core" / "second" / "windows-x64" / "O0"
            duplicate.mkdir(parents=True)
            first = packages / "NativeSharp.Core" / "test" / "windows-x64" / "O0"
            for extension in (".lib", ".nsmeta"):
                shutil.copyfile(first / ("NativeSharpCore" + extension), duplicate / ("NativeSharpCore" + extension))
            result = run(check, binary, env)
            if result.returncode == 0 or "multiple installed packages" not in (result.stdout + result.stderr):
                raise RuntimeError("ambiguous package must be rejected: " + result.stdout + result.stderr)
            print("missing level, local, explicit and ambiguous package controls: PASS", flush=True)
        finally:
            os.chdir(old_cwd)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
