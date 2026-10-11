# Validacion 227 - Frontera lexica de escenarios Sema

## Alcance

`TS-SEMA-167` y `TS-SEMA-193`.

## Causa y correccion

Los cuatro escenarios producen su diagnostico durante lexing. El helper corta
antes del parser, por lo que no existe AST que Sema pueda recibir. El contrato
de escenario declara ahora si debe existir arbol y estos casos exigen su
ausencia, conservando diagnóstico y estado exactos.

Se actualizaron los dos helpers equivalentes para mantener una sola semantica
observable en toda la suite.

## Validacion

No se compilo ni se ejecutaron pruebas por instruccion expresa.
