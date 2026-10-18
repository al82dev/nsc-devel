# Tanda NSC305 GATE-F05B — array-store null-conditional lazy

## Alcance

Esta hoja restaura el contrato de `NullConditionalArrayStore` sin cambiar sintaxis, HIR, MIR,
ABI, NSMETA, diagnósticos ni ownership. El receptor se evalúa una vez; el branch null/no-null se
emite antes de bajar índice y RHS; la rama nula devuelve cero/null sin ejecutar sus efectos.

GATE-F05A field-store permanece cerrado e independiente.

## Causa

La migración L04AL apiló receptor, índice y RHS antes de llamar al emisor del store. El helper
contenía el branch correcto, pero los overrides ya se habían emitido en el bloque anterior.

## Diseño

La máquina LLVM usa cuatro fases:

1. bajar únicamente el receptor;
2. emitir el branch, entrar en el bloque no nulo y apilar el índice;
3. normalizar el índice y apilar el RHS;
4. preservar el chequeo de terminación, emitir bounds/address/store y construir el PHI.

El contexto viaja en `Frame`; los helpers prepare/address/finish no llaman `lowerExpression`, por
lo que no reaparece la recursión eliminada por L04. El orden histórico no nulo se conserva:
receptor, índice, RHS, bounds, address y store.

## Validación requerida

- build focal de `nsc` y `nsc_component_tests`;
- TL-LANG-345: resultado 5, cuatro receptores, dos índices y un RHS; el store y load nulos deben
  omitir índice/RHS;
- repetición O0 con `--no-artifact-cache`;
- regresión de load/store null-conditional y bounds mediante los casos existentes relacionados;
- `validate_source_registration.py` y `git diff --check`;
- auditoría estática: branch antes de los pushes de índice/RHS, cero helper anterior y cero
  `lowerExpression` en los helpers F05B.

No se agrega una prueba C++ duplicada: TL-LANG-345 ya ejecuta el `nsc.exe` real y está registrado
en manifiesto, catálogo y Builder.

## Evidencia de cierre

- Builds `nsc` en los dos árboles focales y `nsc_component_tests`: PASS.
- TL-LANG-345 completo registrado: PASS.
- Contrafactual store-only O0 con `--no-artifact-cache`: compile exit 0, run exit 5, dos
  receptores, un índice y un RHS; la rama null omitió índice y RHS.
- TL-LANG-402, regresión null-conditional de indexador de usuario: PASS.
- `validate_source_registration.py` y `git diff --check`: PASS.
- Helper recursivo anterior: cero ocurrencias; prepare/address/finish tienen exactamente una
  definición y una llamada cada uno.

Logs:

- `A:\temp\gate-f05bc-build-nsc.log`
- `A:\temp\gate-f05bc-build-test-infra-nsc.log`
- `A:\temp\gate-f05bc-build-component-runner.log`
- `A:\temp\gate-f05bc-TL-LANG-345.log`
- `A:\temp\gate-f05bc-StoreOnly-no-cache.log`
- `A:\temp\gate-f05bc-TL-LANG-402.log`
