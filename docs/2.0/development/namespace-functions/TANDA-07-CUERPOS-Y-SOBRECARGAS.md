# Tanda 7: análisis de cuerpos y sobrecargas

## Objetivo

Analizar cuerpos de funciones de namespace con las reglas normales del lenguaje y seleccionar sobrecargas usando tipos reales de argumentos.

## Cambios

- `ProjectMethodBodyModel` recibe el modelo de funciones de namespace.
- Las funciones libres se presentan al analizador como callables sin receptor, sin tipo propietario y con ordinales separados de los miembros.
- Sus cuerpos reutilizan el análisis existente de parámetros, locales, bloques, control de flujo, retornos, conversiones, ownership y definite assignment.
- Las llamadas calificadas/no calificadas usan los candidatos registrados en la tanda 5.
- Se aplican aridad, argumentos nombrados/default, promociones variádicas, conversiones implícitas, ownership y `ref`/`out`/`in`.
- La selección conserva conversiones, correspondencia argumento-parámetro, defaults y tipo de retorno para HIR.
- Se diagnostican aridad inválida, ausencia de conversión y ambigüedad entre imports.
- Un método de clase conserva precedencia sobre una función no calificada del namespace; una llamada calificada siempre conserva el propietario namespace.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`: añade callables y bindings de llamada de namespace.
- `src/Frontend/ProjectMethodBodyModel.cpp`: integra cuerpos y resolución de sobrecargas.
- `src/Frontend/ProjectCompilation.cpp`: conecta el modelo de namespace a la fase de cuerpos.
- `CMakeLists.txt`: registra la prueba focalizada.

## Archivos nuevos

- `tests/unit/NamespaceFunctionBodyTests.cpp`: cubre cuerpo, llamada entre archivos, overload, ambigüedad y conversión inválida.
- Este documento.

## Mejora obtenida

Una función libre ya no es sólo un nombre: su cuerpo participa en la semántica multiparchivo real y una llamada conoce exactamente qué símbolo ejecutará y qué conversiones aplicará. Se reutiliza el analizador maduro sin crear clases contenedoras ni conceder `this` o acceso de tipo.

## Verificación específica

- Construcción incremental exclusiva de `nsc`: correcta.
- Construcción y ejecución exclusiva de `nsc_namespace_function_body_tests`.
- Resultado: `PASS: 10 checks`.
- No se ejecutó baseline, CTest general, fixtures generales ni matriz de optimización.