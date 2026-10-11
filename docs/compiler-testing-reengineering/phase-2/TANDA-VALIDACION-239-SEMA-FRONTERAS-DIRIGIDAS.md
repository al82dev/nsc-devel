# Tanda de validación 239 — fronteras semánticas dirigidas

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN PENDIENTE.

## Causa confirmada

`TS-SEMA-123`, `152`, `161` y `214` intentaban observar invariantes privadas
de Sema mediante textos que el parser vigente rechaza antes de construir el
AST requerido. `TS-SEMA-168` clasificaba como diagnóstico semántico una forma
numérica malformada que pertenece al lexer. Esas pruebas no alcanzaban el
contrato que declaraban.

## Corrección

- Los tipos nominales incompletos de `123` y `214` se instalan directamente
  en el ámbito semántico y ejercitan `validateObjectType` y `analyzeIndex`.
- `152` invoca el análisis de `return` sin función activa.
- `161` observa `loopDepth_` y `currentFunction_` después de cuerpos fallidos,
  conservando la secuencia diagnóstica completa.
- `168` usa un literal exponencial léxicamente inválido y declara de forma
  explícita que no existe AST.

## Alcance

Solo cambia la construcción de los fixtures. No se modifica el compilador ni
se convierte un fallo de producto en aceptación.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
su corrida posterior.
