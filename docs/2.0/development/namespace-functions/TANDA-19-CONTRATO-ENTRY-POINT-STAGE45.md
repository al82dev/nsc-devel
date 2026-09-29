# Tanda 19: sincronizacion del contrato de entry point en Stage 45

## Objetivo

Corregir los dos fallos reportados por `nsc.stage45.linker-pe-eh` sin modificar el comportamiento del compilador ni ejecutar el baseline.

## Diagnostico

Stage 45 seguia buscando literalmente los diagnosticos anteriores a las funciones libres:

- `requires exactly one static int Main()`
- `multiple static int Main() entry points`

El contrato vigente permite como punto de entrada tanto una funcion libre `int Main()` como un metodo `static int Main()`, ambos sin parametros. El compilador ya emitia correctamente los mensajes generalizados, pero las dos aserciones antiguas no se actualizaron junto con el feature.

## Cambios

- La prueba de entry point ausente ahora exige el diagnostico completo que menciona funcion libre o metodo estatico sin parametros.
- La prueba de ambiguedad ahora exige `multiple int Main() entry points`, aplicable a cualquier combinacion de funciones libres y metodos estaticos validos.
- El texto descriptivo de la primera asercion deja de presentar el contrato exclusivamente como un metodo estatico.

## Archivos modificados

- `tests/unit/Stage45LinkerPeEhTests.cpp`: sincroniza las dos expectativas con el contrato actual de seleccion de `Main`.

## Archivo nuevo

- `docs/2.0/development/namespace-functions/TANDA-19-CONTRATO-ENTRY-POINT-STAGE45.md`: registra la causa, el alcance y la verificacion de la correccion.

No se crearon ejecutables, scripts ni directorios permanentes. No se modifico codigo de produccion.

## Mejora obtenida

Stage 45 vuelve a validar de forma precisa el diagnostico real del compilador sin negar el soporte agregado para funciones libres. La prueba conserva las comprobaciones de codigo de salida, ausencia de artefactos incompletos y determinismo del mensaje.

## Verificacion

- Construccion exclusiva de `nsc` y `nsc_stage45_linker_pe_eh_tests`: correcta.
- Ejecucion individual de `nsc.stage45.linker-pe-eh`: PASS en 0.80 s.
- No se ejecuto baseline, suite general ni matriz de optimizaciones.
- Los archivos `error*.log` del baseline externo no se modificaron ni se incluyeron.
