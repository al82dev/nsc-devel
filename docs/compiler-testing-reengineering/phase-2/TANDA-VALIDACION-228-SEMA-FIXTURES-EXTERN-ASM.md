# Validacion 228 - Fixtures extern C e inline asm vigentes

## Alcance

`TS-SEMA-128`, `TS-SEMA-135`, `TS-SEMA-157` y `TS-SEMA-158`.

## Causa y correccion

Los fixtures usaban formas que el parser vigente ya no entrega a Sema. Los
cuerpos extern C se colocan dentro del bloque `extern C { ... }`; inline asm
usa bloque seguido de clausulas `in`/`out`. Los oraculos asm incluyen ademas
el diagnostico terminal `InlineAssemblyNotAvailable` que el analizador emite
despues de validar operandos.

No se modifico parser ni analizador.

## Validacion

No se compilo ni se ejecutaron pruebas por instruccion expresa.
