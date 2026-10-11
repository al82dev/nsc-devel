# NSC Builder 3 modular

This directory contains the autonomous modular entry point for NSC Builder 3.
The current `../nscbuilder3.py` remains available as the previous
implementation, but this package does not import, execute, or load it.

The package owns the canonical `TestCase`, `TestVariant`, `TestExecution`, and
`Run` records. Catalogues, execution, history, reports, and widgets project
those same objects instead of maintaining parallel dictionaries or records.

The dependency direction is explicit: constants and paths; canonical models;
configuration and the LLVM 23 toolchain; catalogue and history; reporting,
process control, build, and execution; then GUI and CLI. Focused tests assert
that this import graph is acyclic and that every mechanically extracted
definition still has the same AST as the established Builder implementation.

`tools/extract_current.py` records the ownership map used for the initial
extraction. It is a development aid only and is never imported at runtime.

Run the modular application from the repository root:

```powershell
python nsc-builder/nscbuilder3.py
python nsc-builder/nscbuilder3.py --audit-catalog
python nsc-builder/nscbuilder3.py --gui-smoke
```

The package does not configure or build NSC unless the operator explicitly
uses those GUI actions.
