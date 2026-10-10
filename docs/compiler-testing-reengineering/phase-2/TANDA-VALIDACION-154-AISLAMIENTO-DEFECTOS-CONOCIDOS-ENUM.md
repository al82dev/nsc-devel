# Tanda de validación 154 — Terminaciones conocidas de enums externos

## Estado

`IMPLEMENTADA; VALIDACIÓN INTEGRAL DIFERIDA HASTA VALIDACIÓN 155`

## Defecto concreto

`TL-LANG-099` y `TL-LANG-250` caracterizan dos defectos de producto ya
inventariados: valores externos de enum alcanzan un índice de tabla sin
validación. En Debug, la biblioteca estándar de C++ termina el proceso con
`Assertion failed: array subscript out of range` antes de que el runner pueda
publicar su sobre JSON. Builder 3 interpretaba la ausencia de JSON como defecto
de infraestructura, aunque cada caso ya estaba aislado en un proceso propio.

## Corrección mínima

Las dos filas declaran ahora un oráculo explícito de terminación anormal:

- el resultado esperado sigue siendo `KNOWN_FAILURE`;
- el proceso debe terminar con código distinto de cero;
- la salida debe contener exactamente la aserción de índice fuera de rango;
- las etiquetas `known-defect` y `unsafe-boundary` son obligatorias.

El cargador canónico rechaza cualquier contrato incompleto o aplicado a un
caso ordinario. Builder 3 sólo transforma en `KNOWN_FAILURE` una terminación
que satisface toda esa identidad. Un código cero, texto distinto, timeout,
caso no marcado o crash arbitrario permanece como `INFRASTRUCTURE_ERROR`.

## Comportamiento preservado

El defecto no se oculta ni se transforma en éxito; continúa visible y rojo
como defecto conocido. No se captura la aserción dentro del mismo proceso, no
se modifica la función de producto y no se debilita el validador del sobre
JSON para los demás casos.

## Evidencia preparada

Las pruebas dirigidas cubren el contrato válido, código cero, texto ajeno,
caso ordinario y mutaciones inválidas del manifiesto. Por instrucción expresa
su ejecución se difiere hasta la corrida única final `Debug/O0/Todos`.
