# NSC Builder 3 modular

This directory contains the modular entry point for NSC Builder 3. The current
`../nscbuilder3.py` remains untouched and is loaded once as the compatibility
runtime so the extracted package preserves the established CLI, GUI, paths,
commands, evidence formats, cancellation rules, and timeout behavior.

The package owns the canonical `TestCase`, `TestVariant`, `TestExecution`, and
`Run` records. They are installed into the compatibility runtime before any
catalogue or GUI object is created, so catalogues, execution, history, reports,
and widgets project the same objects instead of maintaining parallel models.

Run the modular application from the repository root:

```powershell
python nsc-builder/nscbuilder3.py
python nsc-builder/nscbuilder3.py --audit-catalog
python nsc-builder/nscbuilder3.py --gui-smoke
```

The package does not configure or build NSC unless the operator explicitly
uses those existing GUI actions.
