# Tanda documental 02 — fuentes subordinadas y reconciliación del ledger

## Objetivo

Cerrar la divergencia entre el catálogo canónico, los expedientes operativos T29–T49 y el ledger de 156
limitaciones, sin convertir nombres de tandas o pruebas aisladas en certificación de baseline.

## Cambios

- El maestro registra las fuentes raíz `vsBeef`, `para_el_*`, la auditoría PDB, funciones libres, documentación,
  lineamientos, trackers históricos y familias de expedientes posteriores a T28.
- Los roadmaps y trackers secundarios muestran un aviso explícito de subordinación al Documento maestro §24.
- Veinticuatro IDs `NS3-LIM-*` pasan de `ENRUTADA` a `TERMINADA` porque el ledger aporta commit, expediente y
  prueba dirigida; `NS3-LIM-OWN-001` y `NS3-LIM-PKG-002` permanecen abiertos.
- Cada uno de los 308 ítems tiene ahora evidencia o fuente, incluso cuando todavía está pendiente.
- El verificador comprueba los catálogos fuente P00–P11, E001–E054, gates `NS3-I*`, tracks `NS3-X*` y las 26 rutas
  `NS3-LIM-*` contra sus IDs canónicos.
- El ledger debe contener exactamente las secciones 1–156, sin duplicados, con cinco columnas completas, las cuatro
  fuentes v1–v4 y los expedientes Brotli/Bzip2/Libpng/Zlib.
- El verificador impide que los 24 cierres respaldados vuelvan a un estado abierto o que los dos casos aún abiertos
  se marquen terminales sin nueva evidencia.

## Archivos del repositorio

- `tools/native_sharp_3_catalog.py`.
- `tools/verify_native_sharp_3_catalog.py`.
- `tests/unit/test_native_sharp_3_catalog.py`.
- Este expediente.

## Documentos de `A:\` actualizados

- `DOCUMENTO-MAESTRO-NATIVESHARP-3.0.md` y su tracking generado.
- `NATIVESHARP-3.0-LEDGER-LIMITACIONES.md`.
- `native-sharp-3.0-4.0.md`, `vsBeef.md`, `NATIVESHARP-3.0-TRACKING - Copy.md` y
  `nsc-function-namespace.md`.
- `nsc-historia/NativeSharp-3.0-Unified-Language-Compiler-Runtime-Roadmap.md`.

## Validación dirigida

- `tests/unit/test_native_sharp_3_catalog.py`: 7/7.
- `tests/unit/test_verify_native_sharp_3_plan.py`: 6/6.
- Verificador real: 308 ítems, 16 componentes, ledger 1–156 completo y tracking idéntico al maestro.
- No se ejecutó baseline, CTest general, compilación ni instalación.
