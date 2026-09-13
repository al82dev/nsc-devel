# Reparación del cierre de tanda 4: contratos MIR 1.2

## Español

### Motivo

El item 300 elevó correctamente el esquema MIR de `1.0` a `1.2` para conservar los efectos y
clobbers del ensamblador integrado. El compilador, el contrato generado, la especificación y la
documentación ya publicaban `mir_schema=1.2`, pero dos pruebas históricas aún exigían literalmente
`mir_schema=1.0`. Por ello el baseline de cierre de la tanda 4 marcaba dos fallos administrativos
aunque las 340 pruebas funcionales restantes y las matrices O0/O2 pasaban.

### Archivos y cambios

- `tests/unit/Stage62ToolchainContractTests.cpp`
  - Actualiza la identidad MIR esperada a `1.2`.
  - Actualiza el caso negativo que elimina el campo canónico para verificar el diagnóstico de
    campo faltante.
- `tests/unit/Stage69MigrationBaselineTests.cpp`
  - Actualiza la identidad MIR congelada por el baseline de migración a `1.2`.

### Decisión de diseño

Las expectativas continúan siendo literales. Estas pruebas son barreras deliberadas de
compatibilidad: un cambio futuro del esquema MIR debe obligar a revisar conscientemente el contrato
del toolchain y el baseline de migración. No se debilitó la comprobación derivando la versión de la
misma constante que produce el artefacto.

### Evidencia

El baseline que descubrió la desalineación quedó en
`_nscbuilder/history/20260913-133156-db2c946f/report.md`. Los únicos tests CTest fallidos en ambos
backends fueron `nsc.stage62.toolchain-contract` y `nsc.stage69.migration-baseline`; las pruebas
`nsc.debug.assembly-stepping-o0` y `nsc.assembly.abi-c-cpp-o0` pasaron con lld-link y NSLink.

## English

### Reason

Item 300 intentionally raised the MIR schema from `1.0` to `1.2` so integrated-assembly effects and
clobbers survive the MIR boundary. The compiler, generated contract, specification, and current
documentation already published `mir_schema=1.2`, while two historical tests still required the
literal `mir_schema=1.0`. Consequently, the batch 4 closing baseline reported two administrative
failures even though the other 340 functional tests and the O0/O2 matrices passed.

### Files and changes

- `tests/unit/Stage62ToolchainContractTests.cpp`
  - Updates the expected MIR identity to `1.2`.
  - Updates the negative fixture that removes the canonical field and checks the missing-field
    diagnostic.
- `tests/unit/Stage69MigrationBaselineTests.cpp`
  - Updates the MIR identity frozen by the migration baseline to `1.2`.

### Design decision

The expectations remain literal. These tests deliberately guard compatibility: a future MIR schema
change must force an explicit review of both the toolchain contract and migration baseline. The test
was not weakened by deriving its expected version from the same constant that produces the artifact.

### Evidence

The baseline that exposed the mismatch is recorded at
`_nscbuilder/history/20260913-133156-db2c946f/report.md`. The only CTest failures on both backends
were `nsc.stage62.toolchain-contract` and `nsc.stage69.migration-baseline`; the new
`nsc.debug.assembly-stepping-o0` and `nsc.assembly.abi-c-cpp-o0` tests passed with lld-link and
NSLink.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Alinea contratos con MIR 1.2 / Align contracts with MIR 1.2`

**Descripción / Description**

`Actualiza las barreras explícitas del contrato de toolchain y del baseline de migración después del cambio de esquema requerido por asm integrado. / Update the explicit toolchain-contract and migration-baseline guards after the schema change required by integrated assembly.`
