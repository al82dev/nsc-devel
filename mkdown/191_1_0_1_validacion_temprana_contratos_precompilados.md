# 1.0.1 — Validación temprana de contratos precompilados

## Resultado

Las rutas de comprobación HIR, MIR y LLVM validan los artefactos de cada
`LibraryReference` antes de declarar correcto el proyecto. La comprobación no
enlaza ni genera binarios, pero confirma que NSMETA, el archivo COFF y la DLL
opcional forman un contrato materializable.

## Causa raíz

La validación de referencias precompiladas sólo se ejecutaba desde las rutas de
emisión EXE, LIB y DLL. Por eso `--check-project-oop-hir` podía aceptar un
consumidor cuya metadata se resolvía semánticamente, aunque el archivo físico no
publicara los símbolos prometidos. El error aparecía mucho después durante la
emisión.

## Archivos e implementación

- `src/Driver/Driver.cpp` invoca
  `appendValidatedLibraryReferences` después de construir correctamente el HIR
  o cualquier etapa posterior solicitada por `runProject`.
- La comprobación reutiliza exactamente el lector NSMETA y los verificadores
  COFF/PE de las rutas de emisión.
- Como esas acciones no construyen MIR en todos los casos, la validación se
  detiene deliberadamente después de certificar los artefactos. La selección y
  copia de runtimes por alcance sigue perteneciendo a la emisión.
- Un fallo se reporta como `precompiled library contract validation failed`, de
  forma separada de los errores semánticos del lenguaje.

No se modificaron bindings ni tutoriales.

## Verificación diferida

De acuerdo con el flujo de integración actual, este cambio se guardó sin una
compilación intermedia. Se comprobará junto con el barrido completo mediante
una sola reconstrucción final de NSC. No se ejecutó baseline.

## Commit de referencia

### Español

**Título:** Valida contratos precompilados desde HIR

**Descripción:** Ejecuta la validación común de NSMETA, archivo COFF y runtime
opcional en las comprobaciones HIR y posteriores. Evita falsos positivos que
antes sólo se descubrían durante la emisión de un binario.

### English

**Title:** Validate precompiled contracts from HIR

**Description:** Run the shared NSMETA, COFF archive, and optional runtime
validation during HIR and later checks. Prevent false positives that were
previously discovered only while emitting a binary.

## English summary

Consumer-facing project checks now certify precompiled artifact triplets as
soon as HIR has materialized their public contracts, without linking or
packaging runtimes.
