# 392 — Enlace incremental / Incremental linking

`--incremental-link` mantiene junto al EXE un estado `.nslinc` SHA-256 versionado. La clave cubre
todos los objetos, IR de programa y runtime, entry point, optimización, debug, subsistema, imports y
bibliotecas externas. Un hit sólo reutiliza el PE si el sidecar coincide, el PDB requerido existe y
el verificador PE vuelve a aprobar la imagen; cualquier cambio fuerza un enlace normal y publica el
nuevo estado de forma atómica. / `--incremental-link` keeps a versioned SHA-256 `.nslinc` state next
to the EXE. Its key covers every object, program/runtime IR, entry point, modes, imports, and external
libraries. A hit still re-verifies the PE; any change performs a normal link and atomically publishes
new state.
