from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import xml.etree.ElementTree as ET
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, Sequence


PROFILE_SCHEMA = "nativesharp-bootstrap-profile"
PROFILE_VERSION = 1
REPORT_SCHEMA = "nativesharp-bootstrap-profile-report"
REPORT_VERSION = 1
EXIT_COMPLIANT = 0
EXIT_VIOLATION = 2
EXIT_INVALID_INPUT = 3


@dataclass(frozen=True)
class Token:
    text: str
    line: int
    column: int


@dataclass(frozen=True)
class Violation:
    code: str
    path: str
    line: int
    column: int
    message: str


class ProfileError(ValueError):
    pass


def load_profile(path: Path) -> dict:
    try:
        profile = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ProfileError(f"cannot read profile '{path}': {error}") from error

    if profile.get("schema") != PROFILE_SCHEMA or profile.get("version") != PROFILE_VERSION:
        raise ProfileError(
            f"unsupported profile contract: expected {PROFILE_SCHEMA} v{PROFILE_VERSION}"
        )

    required_lists = (
        "known_reserved_keywords",
        "allowed_reserved_keywords",
        "forbidden_tokens",
        "forbidden_sequences",
        "forbidden_namespace_prefixes",
        "allowed_project_references",
    )
    for field in required_lists:
        if not isinstance(profile.get(field), list):
            raise ProfileError(f"profile field '{field}' must be an array")

    known = profile["known_reserved_keywords"]
    allowed = profile["allowed_reserved_keywords"]
    if len(known) != len(set(known)) or len(allowed) != len(set(allowed)):
        raise ProfileError("keyword arrays must not contain duplicates")
    unknown_allowed = sorted(set(allowed) - set(known))
    if unknown_allowed:
        raise ProfileError(f"allowed keywords are not recognized: {', '.join(unknown_allowed)}")

    codes: list[str] = []
    for field in ("forbidden_tokens", "forbidden_sequences", "forbidden_namespace_prefixes"):
        for rule in profile[field]:
            if not isinstance(rule, dict) or not rule.get("code") or not rule.get("message"):
                raise ProfileError(f"every {field} entry requires code and message")
            if re.fullmatch(r"NSBP[0-9]{4}", rule["code"]) is None:
                raise ProfileError(f"invalid profile diagnostic code: {rule['code']}")
            if field == "forbidden_tokens" and not isinstance(rule.get("token"), str):
                raise ProfileError("every forbidden_tokens entry requires a token")
            if field == "forbidden_sequences" and (
                not isinstance(rule.get("tokens"), list)
                or not rule["tokens"]
                or not all(isinstance(token, str) and token for token in rule["tokens"])
            ):
                raise ProfileError("every forbidden_sequences entry requires a non-empty token array")
            if field == "forbidden_namespace_prefixes" and not isinstance(rule.get("prefix"), str):
                raise ProfileError("every forbidden_namespace_prefixes entry requires a prefix")
            codes.append(rule["code"])
    if len(codes) != len(set(codes)):
        raise ProfileError("profile diagnostic codes must be unique")

    forbidden_keywords = {
        rule["token"] for rule in profile["forbidden_tokens"] if rule["token"] in known
    }
    missing_policy = sorted(set(known) - set(allowed) - forbidden_keywords)
    if missing_policy:
        raise ProfileError(
            "recognized keywords require an explicit allowed or forbidden policy: "
            + ", ".join(missing_policy)
        )
    contradictory = sorted(set(allowed) & forbidden_keywords)
    if contradictory:
        raise ProfileError(
            "keywords cannot be both allowed and forbidden: " + ", ".join(contradictory)
        )
    if not all(isinstance(item, str) and item for item in profile["allowed_project_references"]):
        raise ProfileError("allowed_project_references entries must be non-empty strings")
    return profile


def _advance(text: str, index: int, line: int, column: int) -> tuple[int, int, int]:
    if text[index] == "\n":
        return index + 1, line + 1, 1
    return index + 1, line, column + 1


def tokenize(text: str, source_name: str = "<memory>") -> list[Token]:
    tokens: list[Token] = []
    index = 0
    line = 1
    column = 1
    length = len(text)

    while index < length:
        character = text[index]
        if character.isspace():
            index, line, column = _advance(text, index, line, column)
            continue

        if text.startswith("//", index):
            while index < length and text[index] != "\n":
                index, line, column = _advance(text, index, line, column)
            continue

        if text.startswith("/*", index):
            start_line, start_column = line, column
            index, line, column = _advance(text, index, line, column)
            index, line, column = _advance(text, index, line, column)
            while index < length and not text.startswith("*/", index):
                index, line, column = _advance(text, index, line, column)
            if index >= length:
                raise ProfileError(
                    f"{source_name}:{start_line}:{start_column}: unterminated block comment"
                )
            index, line, column = _advance(text, index, line, column)
            index, line, column = _advance(text, index, line, column)
            continue

        if character in ('"', "'"):
            quote = character
            start_line, start_column = line, column
            index, line, column = _advance(text, index, line, column)
            escaped = False
            while index < length:
                current = text[index]
                if not escaped and current == quote:
                    index, line, column = _advance(text, index, line, column)
                    break
                if not escaped and current == "\n":
                    raise ProfileError(
                        f"{source_name}:{start_line}:{start_column}: unterminated literal"
                    )
                escaped = not escaped and current == "\\"
                if current != "\\":
                    escaped = False
                index, line, column = _advance(text, index, line, column)
            else:
                raise ProfileError(
                    f"{source_name}:{start_line}:{start_column}: unterminated literal"
                )
            continue

        start_line, start_column = line, column
        if character.isalpha() or character == "_":
            begin = index
            while index < length and (text[index].isalnum() or text[index] == "_"):
                index, line, column = _advance(text, index, line, column)
            tokens.append(Token(text[begin:index], start_line, start_column))
            continue

        if character.isdigit():
            begin = index
            while index < length and (text[index].isalnum() or text[index] in "._"):
                index, line, column = _advance(text, index, line, column)
            tokens.append(Token(text[begin:index], start_line, start_column))
            continue

        matched = False
        for operator in ("<<=", ">>=", "...", "=>", "->", "==", "!=", "<=", ">=", "++", "--", "&&", "||", "<<", ">>", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^="):
            if text.startswith(operator, index):
                tokens.append(Token(operator, start_line, start_column))
                for _ in operator:
                    index, line, column = _advance(text, index, line, column)
                matched = True
                break
        if matched:
            continue

        tokens.append(Token(character, start_line, start_column))
        index, line, column = _advance(text, index, line, column)

    return tokens


def _matches(tokens: Sequence[Token], index: int, expected: Sequence[str]) -> bool:
    return [token.text for token in tokens[index : index + len(expected)]] == list(expected)


def _namespace_import(tokens: Sequence[Token], index: int) -> tuple[str, int] | None:
    if tokens[index].text != "using" or index + 1 >= len(tokens):
        return None
    if tokens[index + 1].text == "(":
        return None
    cursor = index + 1
    parts: list[str] = []
    expect_identifier = True
    while cursor < len(tokens) and tokens[cursor].text != ";":
        text = tokens[cursor].text
        if expect_identifier and (text[0].isalpha() or text[0] == "_"):
            parts.append(text)
            expect_identifier = False
        elif not expect_identifier and text == ".":
            expect_identifier = True
        else:
            return None
        cursor += 1
    if cursor >= len(tokens) or expect_identifier:
        return None
    return ".".join(parts), cursor


def scan_source(path: Path, profile: dict, display_path: str | None = None) -> list[Violation]:
    shown = display_path or path.as_posix()
    try:
        source = path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        raise ProfileError(f"cannot read source '{path}': {error}") from error
    tokens = tokenize(source, shown)
    violations: list[Violation] = []
    allowed = set(profile["allowed_reserved_keywords"])
    known = set(profile["known_reserved_keywords"])
    token_rules = {rule["token"]: rule for rule in profile["forbidden_tokens"]}

    for index, token in enumerate(tokens):
        rule = token_rules.get(token.text)
        if rule is not None:
            violations.append(Violation(rule["code"], shown, token.line, token.column, rule["message"]))
        elif token.text in known and token.text not in allowed:
            violations.append(
                Violation(
                    "NSBP1099",
                    shown,
                    token.line,
                    token.column,
                    f"reserved keyword '{token.text}' is not approved by Bootstrap Profile 2.0",
                )
            )

        imported = _namespace_import(tokens, index)
        if imported is not None:
            namespace, _ = imported
            for forbidden in profile["forbidden_namespace_prefixes"]:
                prefix = forbidden["prefix"]
                if namespace == prefix or namespace.startswith(prefix + "."):
                    violations.append(
                        Violation(
                            forbidden["code"],
                            shown,
                            token.line,
                            token.column,
                            forbidden["message"],
                        )
                    )

        for sequence_rule in profile["forbidden_sequences"]:
            expected = sequence_rule["tokens"]
            if _matches(tokens, index, expected):
                violations.append(
                    Violation(
                        sequence_rule["code"],
                        shown,
                        token.line,
                        token.column,
                        sequence_rule["message"],
                    )
                )

        for forbidden in profile["forbidden_namespace_prefixes"]:
            expected: list[str] = []
            for part_index, part in enumerate(forbidden["prefix"].split(".")):
                if part_index:
                    expected.append(".")
                expected.append(part)
            if _matches(tokens, index, expected):
                violations.append(
                    Violation(
                        forbidden["code"],
                        shown,
                        token.line,
                        token.column,
                        forbidden["message"],
                    )
                )

    unique = {(v.code, v.path, v.line, v.column, v.message): v for v in violations}
    return sorted(unique.values(), key=lambda item: (item.path.casefold(), item.line, item.column, item.code))


def scan_project(path: Path, profile: dict, display_path: str | None = None) -> list[Violation]:
    shown = display_path or path.as_posix()
    try:
        root = ET.fromstring(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, ET.ParseError) as error:
        raise ProfileError(f"cannot read project '{path}': {error}") from error

    allowed = {name.casefold() for name in profile["allowed_project_references"]}
    violations: list[Violation] = []
    for element in root.iter():
        tag = element.tag.rsplit("}", 1)[-1]
        if tag == "ProjectReference":
            include = element.attrib.get("Include", "")
            project_name = Path(include.replace("\\", "/")).name.casefold()
            if project_name not in allowed:
                violations.append(
                    Violation(
                        "NSBP3001",
                        shown,
                        1,
                        1,
                        f"project reference '{include}' is outside the Bootstrap Profile stdlib allowlist",
                    )
                )
        elif tag in {"PackageReference", "NativeReference"}:
            include = element.attrib.get("Include", "")
            violations.append(
                Violation(
                    "NSBP3002",
                    shown,
                    1,
                    1,
                    f"external reference '{include}' is not permitted during bootstrap",
                )
            )
    return violations


def collect_inputs(roots: Iterable[Path]) -> list[Path]:
    files: set[Path] = set()
    for root in roots:
        if root.is_file():
            if root.suffix.casefold() in {".ns", ".nsproj"}:
                files.add(root.resolve())
            continue
        if not root.is_dir():
            raise ProfileError(f"source root does not exist: {root}")
        for pattern in ("*.ns", "*.nsproj"):
            files.update(path.resolve() for path in root.rglob(pattern) if path.is_file())
    return sorted(files, key=lambda item: item.as_posix().casefold())


def verify(profile_path: Path, roots: Sequence[Path]) -> dict:
    profile = load_profile(profile_path)
    inputs = collect_inputs(roots)
    violations: list[Violation] = []
    common_root: Path | None = None
    if roots:
        try:
            common_root = Path(roots[0]).resolve() if len(roots) == 1 and roots[0].is_dir() else None
        except OSError:
            common_root = None

    for path in inputs:
        shown = path.as_posix()
        if common_root is not None:
            try:
                shown = path.relative_to(common_root).as_posix()
            except ValueError:
                pass
        if path.suffix.casefold() == ".ns":
            violations.extend(scan_source(path, profile, shown))
        else:
            violations.extend(scan_project(path, profile, shown))

    violations.sort(key=lambda item: (item.path.casefold(), item.line, item.column, item.code))
    return {
        "schema": REPORT_SCHEMA,
        "version": REPORT_VERSION,
        "profile": profile["profile"],
        "profile_sha256": hashlib.sha256(profile_path.read_bytes()).hexdigest(),
        "status": "passed" if not violations else "failed",
        "files_checked": len(inputs),
        "violations": [asdict(item) for item in violations],
    }


def write_report(path: Path, report: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Reject NativeSharp sources outside Bootstrap Profile 2.0 before bootstrap."
    )
    default_profile = Path(__file__).with_name("bootstrap-profile.json")
    parser.add_argument("--profile", type=Path, default=default_profile)
    parser.add_argument("--source-root", type=Path, action="append", required=True)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--quiet", action="store_true")
    arguments = parser.parse_args(argv)
    try:
        report = verify(arguments.profile.resolve(), arguments.source_root)
        if arguments.report is not None:
            write_report(arguments.report.resolve(), report)
    except ProfileError as error:
        print(f"verify_profile: error: {error}", file=sys.stderr)
        return EXIT_INVALID_INPUT

    if not arguments.quiet:
        for violation in report["violations"]:
            print(
                f"{violation['path']}:{violation['line']}:{violation['column']}: "
                f"error {violation['code']}: {violation['message']}",
                file=sys.stderr,
            )
        print(
            f"Bootstrap Profile 2.0: {report['status']} "
            f"({report['files_checked']} files, {len(report['violations'])} violations)"
        )
    return EXIT_COMPLIANT if report["status"] == "passed" else EXIT_VIOLATION


if __name__ == "__main__":
    raise SystemExit(main())
