# Tanda NSC305 GATE-F05C — array-load null-conditional lazy

## Alcance

Esta hoja restaura el contrato de `NullConditionalArrayLoad` sin cambiar sintaxis, HIR, MIR, ABI,
NSMETA ni diagnósticos. El receptor se evalúa una vez; el branch null/no-null se emite antes de
bajar el índice; la rama nula devuelve cero/null sin ejecutar efectos del índice.

F05A field-store permanece cerrado y F05B array-store conserva su propio cambio y expediente.

## Causa

La migración L04AJ apiló receptor e índice antes de llamar al emisor del load. El helper contenía
el branch, pero el `indexOverride` ya había sido emitido en el bloque anterior. TL-LANG-345 mezcla
un store y un load nulos y por eso reveló esta segunda causa después de corregir F05B.

## Diseño

La máquina LLVM usa tres fases:

1. bajar únicamente el receptor;
2. emitir el branch, entrar en el bloque no nulo y apilar el índice;
3. normalizar el índice y preservar terminación, bounds, address, load y PHI.

El contexto viaja en `Frame`; los helpers prepare/address/finish no llaman `lowerExpression`.
El orden no nulo histórico permanece receptor, índice, bounds, address y load.

## Validación requerida

- build focal de `nsc` y `nsc_component_tests`;
- TL-LANG-345 completo: resultado 5, cuatro receptores, dos índices y un RHS;
- contrafactual load-only O0 con `--no-artifact-cache`;
- contrafactual store-only para atribuir F05B independientemente;
- `validate_source_registration.py`, `git diff --check` y auditoría de símbolos;
- cero helper anterior y cero `lowerExpression` dentro de los helpers F05C.

No se agrega una prueba C++ duplicada: el caso real y los contrafactuales temporales ejercen
directamente `nsc.exe`.

## Evidencia de cierre

- Builds `nsc` en los dos árboles focales y `nsc_component_tests`: PASS.
- TL-LANG-345 completo registrado: PASS.
- TL-LANG-345 exacto O0 con `--no-artifact-cache`: compile exit 0 y run exit 5; cuatro
  receptores, dos índices y un RHS.
- Contrafactual load-only O0 con `--no-artifact-cache`: compile exit 0, run exit 5, dos
  receptores y un índice; la rama null omitió el índice.
- `validate_source_registration.py` y `git diff --check`: PASS.
- Helper recursivo anterior: cero ocurrencias; prepare/address/finish tienen exactamente una
  definición y una llamada cada uno.

Logs:

- `A:\temp\gate-f05bc-build-nsc.log`
- `A:\temp\gate-f05bc-build-test-infra-nsc.log`
- `A:\temp\gate-f05bc-build-component-runner.log`
- `A:\temp\gate-f05bc-TL-LANG-345.log`
- `A:\temp\gate-f05bc-TL345-no-cache.log`
- `A:\temp\gate-f05bc-LoadOnly-no-cache.log`
