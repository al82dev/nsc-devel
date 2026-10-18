# Tanda NSC 3.0.5 S37F01 — expectativa canónica de member flags

Fecha: 2026-10-17
Rama: `development`
Revisión de partida: `79e33accb5f9464836c1d4dae7403007d4c8475c`

## SCOPE LOCK

| Dimensión | Contrato de la tanda |
|---|---|
| Objetivo | Alinear la expectativa textual de `testMemberFlagsAndStaticAbi` con el dump MIR canónico vigente. |
| Naturaleza | Corrección focal de prueba. |
| Entrada exacta | Campo `const int Seed = 5` de la fixture `Stage37StaticAbi`. |
| Operación que falla | Búsqueda de una subcadena obsoleta que omite `imported=false`. |
| Invariante | La expectativa debe comprobar estrictamente todos los campos canónicos en su orden real. |
| Archivos autorizados | `tests/unit/Stage37StaticInitializationTests.cpp`, este expediente y el tracking externo. |
| Archivos protegidos | Producción, Parser/R01E y `tests/unit/ProjectMethodBodyModelTests.cpp`. |
| Prohibido | Relajar la expectativa, cambiar el dumper, corregir S37F02/S37F03 o ejecutar baseline/suite completa. |
| Terminación | Target Stage37 construye; la batería Stage37 demuestra que el check member-flags deja de fallar y sólo permanecen las causas S37F02/S37F03. |

## Diagnóstico y corrección

El dump observado contiene exactamente:

```text
const=true imported=false constant=true value=5
```

La prueba esperaba `const=true constant=true value=5`, por lo que fallaba únicamente porque el campo
canónico `imported=false` interrumpe esa subcadena. La corrección incorpora el campo y conserva una comparación
estricta; no elimina atributos, no acepta variantes y no modifica producción.

## Validación

- Build `nsc_stage37_static_initialization_tests`: PASS, exit 0, 8.645 s.
  Evidencia: `A:\temp\s37f01-build-stage37.log`.
- CTest `nsc.stage37.static-initialization`: 52/55 checks PASS. El check member-flags ya no
  aparece entre los fallos. Permanecen exactamente un fallo S37F02 y los dos checks S37F03.
  Evidencia: `A:\temp\s37f01-ctest-stage37.log`.

El proceso CTest conserva exit 8 porque S37F02/S37F03 siguen abiertos; no se presenta la batería amplia como
PASS. La evidencia de S37F01 es la eliminación dirigida del único fallo textual de member flags sin modificar
ninguna otra expectativa ni producción.
