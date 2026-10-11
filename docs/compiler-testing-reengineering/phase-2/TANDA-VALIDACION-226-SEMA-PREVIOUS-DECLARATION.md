# Validacion 226 - Notas de declaracion previa en Sema

## Alcance

`TS-SEMA-118`, `TS-SEMA-125`, `TS-SEMA-127` y `TS-SEMA-145`.

## Causa y correccion

Los oraculos esperaban solo el diagnostico primario de duplicado o conflicto.
El contrato del analizador emite inmediatamente despues la nota
`PreviousDeclaration`. Los cuatro escenarios declaran ahora la secuencia
ordenada completa sin relajar el conteo exacto ni modificar producto.

## Validacion

No se compilo ni se ejecutaron pruebas por instruccion expresa.
