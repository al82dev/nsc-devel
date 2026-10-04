# NS3-DOC-01–04 — Catálogo maestro y tracking canónico

## Objetivo

Eliminar los tableros incompatibles de NativeSharp 3.0 y conservar una sola autoridad verificable de estado.

## Cambios

- La sección 24 de `A:\DOCUMENTO-MAESTRO-NATIVESHARP-3.0.md` contiene los 308 ítems canónicos de 16 componentes.
- `A:\NATIVESHARP-3.0-TRACKING.md` es únicamente una tabla compacta generada desde esa sección.
- El catálogo recupera los 120 ítems de NSC y los 47 de NSLink, NSASM, NSDBG, NSC-LSP y VS Code desde el roadmap detallado.
- Se incorporan rendimiento, Runtime/STL, bindings, Project Manager, distribución, Saru, integración, trabajo transversal, limitaciones importadas y gobernanza documental.
- Los estados terminales requieren evidencia y las entregas NSC terminadas requieren documento y commit existentes.
- Los roadmaps y ledgers secundarios declaran expresamente que no son autoridad de estado.
- Los comandos anteriores de generación y validación quedan como compatibilidad y ya no pueden reconstruir el tablero parcial de 167 ítems.

## Archivos

- Nuevo: `tools/native_sharp_3_catalog.py`.
- Nuevo: `tools/verify_native_sharp_3_catalog.py`.
- Nuevo: `tests/unit/test_native_sharp_3_catalog.py`.
- Modificado: `tools/generate_native_sharp_3_tracking.py`.
- Modificado: `tools/verify_native_sharp_3_plan.py`.
- Modificado: `mkdown/NativeSharp-Language-3.0-Roadmap.md`.
- Modificados fuera del repositorio, en `A:\`: documento maestro, tracking, ledger de limitaciones y pendientes por componente.

## Mejora obtenida

Una regeneración ya no pierde pendientes ni conserva dos universos distintos con el mismo total aparente. El
verificador exige exactamente 308 IDs, su componente correcto, estados válidos, evidencia terminal y equivalencia
byte a byte entre el maestro y el tracking visible.

## Validación dirigida

- `tests/unit/test_native_sharp_3_catalog.py`: 5/5.
- `tests/unit/test_verify_native_sharp_3_plan.py`: 6/6.
- `tools/verify_native_sharp_3_plan.py`: 308 ítems, 16 componentes, correcto.
- No se ejecutó baseline ni suite general.
