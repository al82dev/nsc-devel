from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Sequence


SCHEMA = "nativesharp-selfhost-semantic-contract"
REPORT_SCHEMA = "nativesharp-selfhost-sh06-report"
VERSION = 1


class VerificationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def run(executable: Path, action: str, source: Path, *extra: str) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        [str(executable), action, str(source), *extra], stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, check=False,
    )


# EN: The differential oracle. nsc0 analyzes a lone file as its implicit project (the file plus
#     NativeSharp.Core) and dumps a post-monomorphization model: name lookup per module, method
#     callables, locals and one name binding per identifier use. The self-host analyzes the same
#     implicit project from Core's sources (--core-project) and dumps its declarations and typed AST
#     before monomorphization. Both are projected to the facts of the root file they share and those
#     facts are compared: declared types, methods (owner, name, arity), and the (name, type) of every
#     place an identifier or member names. Generic instances of nsc0 are compared by base name, and a
#     self-host binding typed by a generic parameter must name a binding of an nsc0 generic callable.
# ES: El oraculo diferencial. nsc0 analiza un archivo suelto como su proyecto implicito (el archivo
#     mas NativeSharp.Core) y vuelca un modelo posterior a la monomorfizacion: busqueda de nombres por
#     modulo, metodos, locales y una ligadura por uso de identificador. El self-host analiza el mismo
#     proyecto implicito desde las fuentes de Core (--core-project) y vuelca sus declaraciones y el
#     AST tipado antes de monomorfizar. Ambos se proyectan a los hechos del archivo raiz que comparten
#     y esos hechos se comparan: tipos declarados, metodos (dueno, nombre, aridad) y el (nombre, tipo)
#     de cada lugar que nombra un identificador o miembro. Las instancias genericas de nsc0 se
#     comparan por nombre base, y una ligadura del self-host tipada por un parametro generico debe
#     nombrar una ligadura de un callable generico de nsc0.
GENERIC_INSTANCE = re.compile(r"__g\d+__.*$")
PLACE_KINDS = ("local", "parameter", "object-field", "static-field", "namespace-state")


def text_of(result: subprocess.CompletedProcess[bytes]) -> str:
    return canonical_lines(result.stdout).decode("utf-8", errors="strict")


def plain_type(text: str) -> str:
    """builtin:int -> int; type:M::ns.Caja__g1__X__h1 -> Caja; owned Caja<Pieza> -> Caja; ns.Name -> Name."""
    text = text.strip()
    for prefix in ("owned ", "borrowed "):
        if text.startswith(prefix):
            text = text[len(prefix):]
    if text.startswith("builtin:"):
        return text[len("builtin:"):]
    if text.startswith("type:"):
        text = text[len("type:"):]
    if "::" in text:
        text = text.split("::", 1)[1]
    text = GENERIC_INSTANCE.sub("", text)
    if "<" in text:
        text = text[:text.index("<")]
    return text.rsplit(".", 1)[-1]


def legacy_facts(symbols: str, sema: str) -> dict:
    modules = re.findall(r"^module\[(\d+)\] (\S+)", symbols, flags=re.MULTILINE)
    require(bool(modules), "nsc0 symbol dump has no module")
    module = max(modules, key=lambda item: int(item[0]))[1]
    types: set[tuple[str, str, bool]] = set()
    for line in symbols.splitlines():
        match = re.match(r"\s+\w+ (class|struct|interface|enum|union) (\S+) key=(\S+?)::", line)
        if match and match.group(3) == module:
            name = match.group(2)
            types.add((match.group(1), GENERIC_INSTANCE.sub("", name).rsplit(".", 1)[-1],
                       bool(GENERIC_INSTANCE.search(name))))
    methods: set[tuple[str, str, int, bool]] = set()
    generic_callables: set[int] = set()
    bindings: set[tuple[str, str]] = set()
    generic_names: set[str] = set()
    section = ""
    for line in sema.splitlines():
        if not line.startswith(" "):
            section = line.split("=")[0].rstrip(":")
            continue
        if section == "callables":
            match = re.match(r"\s+\[(\d+)\] (\S+?)::(\S+)\((.*)\) -> (\S+)", line)
            if match and match.group(2) == module:
                owner, name = match.group(3).rsplit(".", 1)
                generic = bool(GENERIC_INSTANCE.search(owner)) or bool(GENERIC_INSTANCE.search(name))
                if generic:
                    generic_callables.add(int(match.group(1)))
                arity = len([value for value in match.group(4).split(",") if value.strip()])
                if plain_type(owner) != GENERIC_INSTANCE.sub("", name):
                    methods.add((plain_type(owner), GENERIC_INSTANCE.sub("", name), arity, generic))
        elif section == "name-bindings":
            match = re.search(r"callable=(\d+) .*?kind=(\S+) name=(\S+) type=(\S+)", line)
            if match and match.group(2) in PLACE_KINDS:
                if int(match.group(1)) in generic_callables:
                    generic_names.add(match.group(3))
                else:
                    bindings.add((match.group(3), plain_type(match.group(4))))
    return {"types": types, "methods": methods, "bindings": bindings, "generic_names": generic_names}


def candidate_facts(symbols: str, sema: str) -> dict:
    types: set[tuple[str, str, bool]] = set()
    methods: set[tuple[str, str, int, bool]] = set()
    owners: list[tuple[str, bool]] = []
    generic_parameters: set[str] = set()
    for line in symbols.splitlines():
        depth = (len(line) - len(line.lstrip(" "))) // 2
        match = re.match(r"\s*(Class|Struct|Interface|Enum|Union|Method|Namespace|Function|Constructor) (\S+)"
                         r"(?: : (.*))?", line)
        if not match:
            continue
        kind, name, rest = match.group(1), match.group(2), match.group(3) or ""
        owners = owners[:depth]
        generic_list = re.search(r"generic=<<?([^>]*)>", rest)
        if generic_list:
            generic_parameters.update(value.strip() for value in generic_list.group(1).split(",") if value.strip())
        generic = generic_list is not None
        if kind in ("Class", "Struct", "Interface", "Enum", "Union"):
            types.add((kind.lower(), name, generic))
            owners.append((name, generic))
        elif kind == "Method":
            signature = re.match(r"\((.*?)\) ->", rest)
            arity = len([value for value in signature.group(1).split(",") if value.strip()]) if signature else 0
            owner = owners[-1] if owners else ("", False)
            methods.add((owner[0], name, arity, generic or owner[1]))
        else:
            owners.append((name, False))
    enums = {name for kind, name, _ in types if kind == "enum"}
    bindings: set[tuple[str, str]] = set()
    generic_names: set[str] = set()
    error_nodes = 0
    for line in sema.splitlines():
        if ": error" in line:
            error_nodes += 1
        match = re.match(r"\s*(?:\w+: )?(?:IdentifierExpression|MemberExpression) \"([^\"]+)\" : (.+?) "
                         r"(place|value)\b.* symbol=", line)
        if not match:
            continue
        name, typed, category = match.group(1), plain_type(match.group(2)), match.group(3)
        if category == "value" and typed not in enums:
            continue
        if typed in generic_parameters:
            generic_names.add(name)
        else:
            bindings.add((name, typed))
    return {"types": types, "methods": methods, "bindings": bindings, "generic_names": generic_names,
            "error_nodes": error_nodes}


def fact_differences(legacy: dict, candidate: dict) -> list[str]:
    problems: list[str] = []
    plain_legacy = {(kind, name) for kind, name, generic in legacy["types"] if not generic}
    plain_candidate = {(kind, name) for kind, name, generic in candidate["types"] if not generic}
    if plain_legacy != plain_candidate:
        problems.append(f"types nsc0-only={sorted(plain_legacy - plain_candidate)} "
                        f"self-host-only={sorted(plain_candidate - plain_legacy)}")
    generic_legacy = {(kind, name) for kind, name, generic in legacy["types"] if generic}
    generic_candidate = {(kind, name) for kind, name, generic in candidate["types"] if generic}
    if not generic_legacy <= generic_candidate:
        problems.append(f"generic types missing={sorted(generic_legacy - generic_candidate)}")
    methods_legacy = {(owner, name, arity) for owner, name, arity, generic in legacy["methods"] if not generic}
    methods_candidate = {(owner, name, arity) for owner, name, arity, generic in candidate["methods"]
                         if not generic}
    if methods_legacy != methods_candidate:
        problems.append(f"methods nsc0-only={sorted(methods_legacy - methods_candidate)} "
                        f"self-host-only={sorted(methods_candidate - methods_legacy)}")
    generic_methods_legacy = {(owner, name) for owner, name, _, generic in legacy["methods"] if generic}
    generic_methods_candidate = {(owner, name) for owner, name, _, generic in candidate["methods"] if generic}
    if not generic_methods_legacy <= generic_methods_candidate:
        problems.append(f"generic methods missing={sorted(generic_methods_legacy - generic_methods_candidate)}")
    if not legacy["bindings"] <= candidate["bindings"]:
        problems.append(f"bindings missing in self-host={sorted(legacy['bindings'] - candidate['bindings'])}")
    extra = {binding for binding in candidate["bindings"] - legacy["bindings"]
             if binding[0] not in legacy["generic_names"]}
    if extra:
        problems.append(f"self-host bindings outside nsc0={sorted(extra)}")
    if not candidate["generic_names"] <= legacy["generic_names"]:
        problems.append("generic-typed names outside nsc0 generic callables="
                        f"{sorted(candidate['generic_names'] - legacy['generic_names'])}")
    return problems


def candidate_dumps(executable: Path, source: Path, core: Path) -> tuple[str, str]:
    symbols = run(executable, "--dump-symbols", source, "--core-project", str(core))
    sema = run(executable, "--dump-sema", source, "--core-project", str(core))
    for result, action in ((symbols, "--dump-symbols"), (sema, "--dump-sema")):
        require(result.returncode == 0, f"{executable.name} rejected {action} {source.name} with NativeSharp.Core")
        require(result.stderr == b"", f"{executable.name} wrote stderr for {action} {source.name}")
    return text_of(symbols), text_of(sema)


def canonical_lines(data: bytes) -> bytes:
    return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_contract(path: Path) -> dict:
    contract = json.loads(path.read_text(encoding="utf-8"))
    require(contract.get("schema") == SCHEMA, "unexpected semantic contract schema")
    require(contract.get("version") == VERSION, "unsupported semantic contract version")
    require(len(contract.get("differential_cases", [])) >= 12,
            "semantic differential matrix is incomplete")
    require(len(contract.get("invariants", [])) >= 6,
            "semantic invariant matrix is incomplete")
    require(len(contract.get("corpus_roots", [])) >= 6,
            "semantic corpus roots are incomplete")
    return contract


def source_corpus(root: Path, corpus_roots: Sequence[str]) -> list[Path]:
    result: list[Path] = []
    for relative_root in corpus_roots:
        corpus_root = root / relative_root
        require(corpus_root.is_dir(), f"semantic corpus root is missing: {relative_root}")
        for path in corpus_root.rglob("*.ns"):
            relative = path.relative_to(root).as_posix()
            if not relative.startswith("tests/cases/"):
                result.append(path)
    return sorted(result)


def verify(
    root: Path, nsc0: Path, candidates: dict[str, Path],
    contract_path: Path, report_dir: Path,
) -> dict:
    contract = load_contract(contract_path)
    require(bool(candidates), "semantic verifier requires at least one selected profile")
    actions = contract["actions"]
    core = root / contract["core_project"]
    require(core.is_file(), f"NativeSharp.Core project is missing: {contract['core_project']}")
    differential_comparisons = 0
    differential_facts = 0
    for relative in contract["differential_cases"]:
        source = root / relative
        legacy_symbols = run(nsc0, "--dump-symbols", source)
        legacy_sema = run(nsc0, "--dump-sema", source)
        require(legacy_symbols.returncode == 0 and legacy_sema.returncode == 0,
                f"nsc0 did not produce a semantic oracle: {relative}")
        legacy = legacy_facts(text_of(legacy_symbols), text_of(legacy_sema))
        for optimization, executable in candidates.items():
            actual = candidate_facts(*candidate_dumps(executable, source, core))
            require(actual["error_nodes"] == 0, f"{optimization} typed an expression as error in {relative}")
            problems = fact_differences(legacy, actual)
            require(not problems, f"{optimization} semantic facts differ from nsc0 in {relative}: "
                    + "; ".join(problems))
            differential_comparisons += 1
        differential_facts += (len(legacy["types"]) + len(legacy["methods"]) + len(legacy["bindings"]) +
                               len(legacy["generic_names"]))
    require(differential_facts >= int(contract["differential_minimum_facts"]),
            f"semantic differential compares too few facts: {differential_facts}")

    # EN: Negative controls. The comparison must see a changed declaration (the self-host reads a copy of a
    #     case where one local has another type), and both compilers must reject an unresolved name.
    # ES: Controles negativos. La comparacion debe ver una declaracion cambiada (el self-host lee una copia de
    #     un caso donde un local tiene otro tipo), y ambos compiladores deben rechazar un nombre sin resolver.
    control_dir = report_dir / "sh06-controls"
    control_dir.mkdir(parents=True, exist_ok=True)
    mutation = contract["mutation_control"]
    original = root / mutation["source"]
    original_text = original.read_text(encoding="utf-8")
    require(original_text.count(mutation["from"]) == 1, "mutation control text is not unique")
    mutated = control_dir / original.name
    mutated.write_text(original_text.replace(mutation["from"], mutation["to"]), encoding="utf-8")
    legacy = legacy_facts(text_of(run(nsc0, "--dump-symbols", original)), text_of(run(nsc0, "--dump-sema", original)))
    negative_controls = 0
    for optimization, executable in candidates.items():
        changed = candidate_facts(*candidate_dumps(executable, mutated, core))
        require(bool(fact_differences(legacy, changed)),
                f"{optimization} semantic comparison did not detect the mutation control")
        negative_controls += 1
    unresolved = control_dir / "unresolved.ns"
    unresolved.write_text(contract["unresolved_control"], encoding="utf-8")
    require(run(nsc0, "--dump-sema", unresolved).returncode != 0, "nsc0 accepted the unresolved control")
    for optimization, executable in candidates.items():
        result = run(executable, "--dump-sema", unresolved, "--core-project", str(core))
        require(result.returncode != 0, f"{optimization} accepted the unresolved control")
        negative_controls += 1

    sources = source_corpus(root, contract["corpus_roots"])
    require(len(sources) >= int(contract["stability_minimum_sources"]),
            f"semantic stability corpus unexpectedly small: {len(sources)}")
    stability_comparisons = 0
    for source in sources:
        relative = source.relative_to(root).as_posix()
        for action in actions:
            baseline: tuple[int, bytes, bytes] | None = None
            for optimization, executable in candidates.items():
                result = run(executable, action, source)
                require(result.returncode in (0, 1),
                        f"{optimization} semantic process crashed: {action} {relative}")
                observed = (
                    result.returncode,
                    canonical_lines(result.stdout),
                    canonical_lines(result.stderr),
                )
                if baseline is None:
                    baseline = observed
                else:
                    require(observed == baseline,
                            f"semantic output differs across selected profiles: {action} {relative}")
                    stability_comparisons += 1

    invariant_checks = 0
    for invariant in contract["invariants"]:
        source = root / invariant["source"]
        for optimization, executable in candidates.items():
            # EN: Symbol and typed-AST invariants analyze the source as its implicit project, like the differential
            #     cases. The layout metadata dump names internal type ids, which NativeSharp.Core would shift, and
            #     describes only the file's own types, so it reads the file alone.
            # ES: Las invariantes de simbolos y AST tipado analizan la fuente como su proyecto implicito, como los
            #     casos diferenciales. El volcado de metadatos de layout nombra ids internos de tipo, que
            #     NativeSharp.Core desplazaria, y describe solo los tipos del archivo, asi que lee el archivo solo.
            extra = () if invariant["action"] == "--dump-semantic-metadata" else ("--core-project", str(core))
            actual = run(executable, invariant["action"], source, *extra)
            require(actual.returncode == 0,
                    f"{optimization} rejected invariant source: {invariant['source']}")
            text = canonical_lines(actual.stdout).decode("utf-8", errors="strict")
            for expected in invariant["contains"]:
                require(expected in text,
                        f"{optimization} missing semantic invariant in {invariant['source']}: {expected}")
                invariant_checks += 1

    return {
        "schema": REPORT_SCHEMA,
        "version": VERSION,
        "status": "passed",
        "optimizations": list(candidates),
        "source_files": len(sources),
        "differential_case_files": contract["differential_cases"],
        "differential_comparisons": differential_comparisons,
        "differential_facts": differential_facts,
        "negative_controls": negative_controls,
        "stability_comparisons": stability_comparisons,
        "invariant_checks": invariant_checks,
        "binary_sha256": {profile: digest(path) for profile, path in candidates.items()},
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Verify the self-hosted SH06 semantic model.")
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--nsc0", type=Path, required=True)
    for level in ("o0", "o1", "o2", "o3"):
        parser.add_argument(f"--nsc-next-{level}", type=Path)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    arguments = parser.parse_args(argv)
    candidates = {
        level.upper(): value.resolve()
        for level in ("o0", "o1", "o2", "o3")
        if (value := getattr(arguments, f"nsc_next_{level}")) is not None
    }
    try:
        report = verify(
            arguments.source_root.resolve(), arguments.nsc0.resolve(),
            candidates, arguments.contract.resolve(), arguments.report.resolve().parent,
        )
        arguments.report.resolve().write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    except (OSError, subprocess.SubprocessError, json.JSONDecodeError, VerificationError) as error:
        print(f"verify_semantics: error: {error}", file=sys.stderr)
        return 1
    print(
        f"SH06 semantics verified: {report['source_files']} sources, "
        f"{report['differential_comparisons']} differential ({report['differential_facts']} facts, "
        f"{report['negative_controls']} negative controls), "
        f"{report['stability_comparisons']} stability and "
        f"{report['invariant_checks']} invariant comparisons"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
