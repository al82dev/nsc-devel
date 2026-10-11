# Tanda de validación 182 — TP-PAR-026

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente el oráculo de `TP-PAR-026` y sincroniza su
plan, inventario y manifiesto. No modifica el parser ni otras pruebas.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

`looksLikeVariableDeclaration()` reconoce deliberadamente el prefijo
inequívoco `Identificador Identificador` aun cuando el primer identificador no
esté registrado como tipo. El parser debe conservar la forma de declaración y
dejar que la resolución semántica emita después el diagnóstico causal de tipo
desconocido. El oráculo anterior confundía lookahead sintáctico con name
lookup.

## Corrección

La prueba exige ahora que tanto `T value;` con `T` conocido como
`Mystery value;` lleguen por la ruta de declaración. Se sincronizaron el plan,
el inventario y el manifiesto.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
