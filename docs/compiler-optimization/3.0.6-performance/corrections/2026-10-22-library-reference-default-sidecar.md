# Compatibilidad de inicialización de `LibraryReference`

## SCOPE LOCK

- Objetivo: conservar los inicializadores agregados históricos de tres campos de `LibraryReference`.
- Naturaleza: corrección no funcional de compatibilidad de fuente introducida por T4.4.
- Revisión fallida: `e72d116aa`.
- Productor responsable: `LibraryReference::productSidecarPath`.
- Archivo de producto autorizado: `include/nativesharp/Compilation/LibraryReference.h`.
- Protegidos: pruebas, fixtures, CMake, NSC Builder, sintaxis, semántica, diagnósticos, CLI, ABI del lenguaje,
  NSMETA, HIR, MIR, LLVM IR y artefactos emitidos.
- Validación autorizada: limpieza, compilación y ejecución de la batería unitaria completa.
- Prohibido: pruebas externas, minibaseline, baseline, instalación y deploy.
- Terminación: los cuatro inicializadores históricos compilan sin alterar las pruebas y la batería unitaria completa
  pasa.

## Evidencia

La construcción limpia de `nsc_component_tests` diagnosticó cuatro veces
`-Werror,-Wmissing-field-initializers`: líneas 230, 285 y 305 de
`CompilationCore001To112Module.cpp`, y línea 179 de `CompilationGraph113To168Module.cpp`.

El commit `d819f2b97` añadió `productSidecarPath` como cuarto campo agregado. Esos consumidores inicializan el
contrato histórico formado por biblioteca, metadata y runtime; la ausencia del sidecar es válida y debe conservar
el fallback exacto, pero el nuevo campo no declaró explícitamente ese valor predeterminado.

## Corrección

Se añade un inicializador de miembro vacío exclusivamente a `productSidecarPath`. `LibraryReference` continúa
siendo un agregado. Los productores que proporcionan el cuarto campo conservan su valor; los inicializadores
históricos de tres campos obtienen una ruta vacía, que ya representa la ausencia opcional del sidecar.

## Contratos conservados

- El contrato de biblioteca de tres artefactos permanece aceptado.
- La ruta del sidecar sigue siendo opcional y no cambia su descubrimiento ni consumo.
- No cambian sintaxis, semántica, diagnósticos, CLI, ABI del lenguaje, NSMETA ni artefactos emitidos.
- No se modifica ninguna prueba ni consumidor.

## Validación

Estado actual: `CORRECCIÓN COMPILADA, EJECUCIÓN UNITARIA COMPLETA PENDIENTE`.

Se limpió `build-compiler-suite-v3/debug`: Ninja eliminó 296 archivos y terminó con código 0. En la reconstrucción
desde cero, `CompilationCore001To112Module.cpp` y `CompilationGraph113To168Module.cpp` compilaron en los pasos 289
y 291. Ninguna de las cuatro advertencias de inicializador incompleto volvió a aparecer.

La construcción avanzó hasta 304 de 784 objetos y se detuvo exclusivamente por la causa independiente de
compatibilidad de `DriverExecutor`. Por ello la ejecución unitaria completa continúa pendiente; esa segunda causa
no pertenece a esta hipótesis ni a este commit.
