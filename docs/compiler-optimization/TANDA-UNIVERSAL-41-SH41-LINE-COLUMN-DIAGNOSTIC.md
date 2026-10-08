# Tanda universal 41: diagnóstico estable de línea y columna en SH41

## Problema confirmado

Después de corregir el CFG de expresiones condicionales, SH41 ya genera y enlaza
`nsc2-o0-nslink.exe`. La auto-prueba expone ahora una divergencia entre generaciones:

- `nsc1.exe --self-test-basic` devuelve `42`;
- `nsc2-o0-nslink.exe --self-test-basic` devuelve `16`.

El código `16` corresponde exclusivamente a la traducción de offset a línea y columna de
`SourceBuffer.GetLineColumn`. El retorno numérico anterior no conservaba los valores que produjo el
binario self-hosted y no permitía separar un defecto de arreglo, indexación o parámetros `out`.

## Cambio

La rama de error de esa comprobación ahora escribe `line` y `column` antes de devolver `16`. La ruta
correcta no cambia su salida, su código de retorno ni su trabajo observable.

Este cambio es instrumentación diagnóstica permanente: si una generación futura vuelve a divergir,
SH41 mostrará el estado que causó la falla en lugar de limitarse a un número opaco.

## Archivos

- Modificado: `selfhost/nsc/Driver/Program.ns`.
- Nuevo: este documento.

## Validación dirigida

1. Regenerar únicamente SH41 en O0 con las herramientas de prueba instaladas en A:.
2. Ejecutar `--self-test-basic` en `nsc1.exe` y `nsc2-o0-nslink.exe`.
3. Usar los valores reportados para corregir el lowering general; no modificar la expectativa válida
   `line=3, column=1`.
