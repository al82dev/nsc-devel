# Baseline 2026-10-04: frontera dual del COM thunk

## Problema

El verificador del item 341 todavía buscaba el nombre genérico `boundary.catchpad`. La frontera
actual separa correctamente excepciones ordinarias y pánicos de NativeSharp en
`boundary.exception.catchpad` y `boundary.panic.catchpad`.

## Cambio

La prueba exige ahora el `catchswitch` común y los dos `catchpad` concretos. Se conserva la
comprobación del trap `CallbackExceptionEscape` y las ejecuciones O0/O2. Las dos ejecuciones
guardan primero el resultado y después construyen el mensaje, para que un fallo muestre el
diagnóstico real sin depender del orden de evaluación de argumentos de C++.

## Archivo

- `tests/unit/Item341ExceptionSafeComThunkTests.cpp`

## Mejora

El contrato comprueba la frontera completa que realmente protege llamadas COM/extern C y deja de
confundir una ampliación deliberada del modelo de pánico con la ausencia del guard.

## Validación dirigida

- `nsc.item341.exception-safe-com-thunk`: 5/5 comprobaciones correctas con NSLink.
