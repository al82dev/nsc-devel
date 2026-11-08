from pathlib import Path
import sys

source, destination = map(Path, sys.argv[1:3])
mode = sys.argv[3]
for name in ("Api.h", "Api.c"):
    text = (source / name).read_text(encoding="utf-8")
    expected = 5 if name == "Api.h" else 10
    if text.count("_Atomic(") != expected:
        raise SystemExit(f"Cantidad inesperada de declaraciones atomicas: {name}")
    if mode == "tls":
        text = text.replace("_Atomic(enum State) state", "_Thread_local _Atomic(enum State) state")
        text = text.replace("_Atomic(enum SignedState) signed_state", "_Thread_local _Atomic(enum SignedState) signed_state")
        text = text.replace("_Atomic(enum WideState) wide_state", "_Thread_local _Atomic(enum WideState) wide_state")
        text = text.replace("_Atomic(enum TinyState) tiny_state", "_Thread_local _Atomic(enum TinyState) tiny_state")
        text = text.replace("_Atomic(enum ShortState) short_state", "_Thread_local _Atomic(enum ShortState) short_state")
    (destination / name).write_text(text, encoding="utf-8")
