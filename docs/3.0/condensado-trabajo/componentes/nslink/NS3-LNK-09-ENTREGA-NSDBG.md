# NS3-LNK-09 — Entrega a NSDBG / NSDBG handoff

## Español

### Resultado

NSLink entrega ahora a NSDBG un paquete reproducible para las cuatro variantes
de optimización: O0, O1, O2 y O3. Cada variante contiene:

- `debug_oN.exe`, enlazado exclusivamente por NSLink;
- `debug_oN.pdb`, con identidad RSDS y el símbolo `NsdbgHandoffProbe`;
- `debug_oN.map`, con base y tamaño de imagen, secciones, contribuciones
  CodeView y símbolos con RVA/VA;
- resultado esperado 42, declarado en `nsdbg-handoff.json` y comprobado al
  ejecutar el PE.

`/map` y `/map:<ruta>` son funcionalidad productiva del driver. El mapa se
genera antes de publicar y forma parte de la misma transacción atómica y del
caché de artefactos que el PE/PDB. Un observador no puede recibir un PE nuevo
con un mapa viejo.

### Prueba negativa de lld-link

El gate coloca un ejecutable venenoso llamado `lld-link.exe` al principio de
`PATH`. Si cualquier etapa intenta usarlo, escribe un sentinel y termina con
error 97. Los cuatro enlaces O0–O3 terminan correctamente y el sentinel debe
seguir ausente. La evidencia legible queda en `lld-link-negative.txt`.

No se usa `lld-link` como oráculo, fallback ni productor de fixtures. El
compilador C++ sólo produce objetos (`/c`); el único proceso que convierte esos
objetos en PE/PDB/map es NSLink.

### Archivos

- `src/driver/CompatibleDriver.h/.cpp`: contrato `/map`, generación,
  validación, publicación y caché.
- `tests/driver/DriverSdkTests.cpp`: parseo de ruta implícita y explícita.
- `tests/handoff/NsdbgHandoffFixture.cpp`: debuggee freestanding que devuelve
  42.
- `tests/handoff/LldLinkGuard.cpp`: fallback venenoso.
- `tests/handoff/NsdbgHandoffExpected.json`: contrato consumible por NSDBG.
- `tests/handoff/RunNsdbgHandoffGate.cmake`: matriz dirigida O0–O3.
- `tests/CMakeLists.txt`: registro del único gate de este ítem.

### Evidencia dirigida

- `nsltest.nsl32.driver-sdk.exe`: **166/166 checks**.
- `RunNsdbgHandoffGate.cmake`: **PASS O0–O3**; cuatro PE, cuatro PDB, cuatro
  mapas, cuatro salidas 42, cuatro búsquedas DbgHelp y cero invocaciones de
  `lld-link`.
- No se ejecutó baseline, mini-baseline ni CTest general.

## English

### Result

NSLink now hands NSDBG a reproducible O0/O1/O2/O3 fixture bundle. Every
variant contains an NSLink-only PE, its RSDS-matched PDB, an image
map, and the expected exit code 42 declared by a versioned JSON manifest.
DbgHelp must resolve `NsdbgHandoffProbe` from every PDB.

`/map` and `/map:<path>` are production driver features. The map is validated,
published atomically, and cached as part of the same product as the PE/PDB, so
a consumer cannot observe a new image with a stale map.

### Negative lld-link proof

The focused gate places a poison executable named `lld-link.exe` first on
`PATH`. Any accidental fallback writes a sentinel and exits with code 97. All
four links complete and the sentinel remains absent. The C++ compiler is only
allowed to emit objects (`/c`); NSLink is the sole PE/PDB/map producer.

### Focused evidence

- Driver contract: **166/166 checks**.
- NSDBG handoff gate: **PASS O0–O3**, including execution, DbgHelp symbol
  lookup, image-map validation, expected manifest, and negative linker proof.
- No baseline, mini-baseline, or general CTest run was performed.
