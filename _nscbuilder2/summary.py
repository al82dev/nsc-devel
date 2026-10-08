"""Causal error condensation for Builder 2 logs."""

from __future__ import annotations

import re


ERROR_SIGNAL = re.compile(
    r"(?:cmake error|fatal error|\berror:|\bfailed\b|traceback|assertionerror|exception)",
    re.IGNORECASE,
)

ZERO_FAILURE_SIGNAL = re.compile(
    r"(?:^failed\s*=\s*0$|\b0\s+failed\b|\b100%\s+tests\s+passed\b)",
    re.IGNORECASE,
)


def condensed_errors(text: str, limit: int = 200) -> list[str]:
    """Return stable, de-duplicated diagnostic lines without rewriting them."""

    selected: list[str] = []
    seen: set[str] = set()
    for raw in text.splitlines():
        line = raw.strip()
        if (
            not line
            or ZERO_FAILURE_SIGNAL.search(line)
            or not ERROR_SIGNAL.search(line)
        ):
            continue
        normalized = re.sub(r"\s+", " ", line).casefold()
        if normalized in seen:
            continue
        seen.add(normalized)
        selected.append(line)
        if len(selected) >= limit:
            break
    return selected


def format_condensed(errors: list[str]) -> str:
    if not errors:
        return "No causal errors recorded."
    lines = [f"Condensed causal report: {len(errors)} unique signal(s)", ""]
    lines.extend(f"{index}. {line}" for index, line in enumerate(errors, start=1))
    return "\n".join(lines)
