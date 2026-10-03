# NS3-T01-F02 — Parámetros variádicos tipados

## Resultado

`params ReadOnlySpan<T>` ya recorría parser, símbolos, overload resolution, HIR, MIR, LLVM y
NSMETA. La auditoría encontró y corrigió un hueco de lifetime: una llamada que expandía elementos
y devolvía una vista podía ocultar que el backing storage era un `alloca` del caller.

El modelo de cuerpos ahora reconoce una expansión dentro de una expresión de vista. Un retorno
directo se rechaza de forma causal; si la vista se guarda primero en un local, el local conserva
el origen de frame y la regla ordinaria de vistas impide retornarlo después.

## Cambios

- propagación de procedencia stack para llamadas con `selectedUsesExpandedParameterPack`;
- cobertura de llamadas de método y funciones libres, incluida composición anidada;
- diagnóstico de retorno directo desde un pack expandido;
- enraizamiento de locales de vista inicializados desde un pack expandido;
- prueba O0 aislada de cero/uno/muchos, orden de efectos, forwarding, `noheap` y overload fijo;
- inspección LLVM de `params.elements`, `params.span` y ausencia de llamada al allocator;
- rechazo de elementos ownership/non-copyable y de escape directo o indirecto;
- guía bilingüe de costos y diferencias frente al arreglo implícito de C#.

## Archivos

### Productivos modificados

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
- `src/Frontend/ProjectMethodBodyModel.cpp`

### Nuevos

- `tests/unit/NativeSharp3T01TypedParameterPackTests.cpp`: prueba vertical exclusiva de F02.
- `docs/3.0/language/typed-parameter-packs.md`: contrato ES/EN de uso y costos.
- `docs/3.0/items/NS3-T01-F02-typed-parameter-packs.md`: expediente del ítem.

### Infraestructura modificada

- `CMakeLists.txt`: registra el target dirigido, sin incorporarlo a ningún baseline especial.

## Mejora

La expansión conserva su ventaja de costo —stack, stores, cero heap— sin permitir que el puntero
del span sobreviva al frame que contiene sus elementos. La prueba separa además esta semántica de
la fixture compartida de inicializadores y detecta una regresión con un nombre de test propio.

## Pruebas dirigidas

- construcción local de `nsc`: correcta.
- `nsc_ns3_t01_typed_parameter_pack_tests`: `PASS: 6 checks`.
- optimización NativeSharp: sólo `O0`.
- linker de programas `.ns`: `A:\nsc-devel\build-test-nslink\bin\nslink.exe`.
- baseline, CTest completo, O1, O2 y O3: no ejecutados.
