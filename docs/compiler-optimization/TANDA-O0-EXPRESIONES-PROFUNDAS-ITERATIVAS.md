# Tanda O0: análisis iterativo de expresiones profundas

## Problema

`NativeSharp.Math.Functions` terminaba con `0xC00000FD` durante `method-bodies` al analizar el método
`Simplex` de `Noise.ns`. La expresión afectada combina llamadas anidadas a `Permute`, operadores binarios,
accesos a miembros y construcciones de `Vector4`.

La inspección del ejecutable O0 mostró que `ProjectMethodBodyModel::analyzeExpression` reservaba 54,344 bytes
por invocación. La profundidad válida de la expresión multiplicaba ese marco mediante descenso recursivo hasta
agotar la pila de 1 MiB del proceso. No era agotamiento de memoria del equipo ni un ciclo semántico.

## Cambio

- Se separó el despachador público `analyzeExpression` del núcleo semántico existente.
- Las cadenas profundas de llamadas ordinarias, operadores binarios no lógicos y construcciones se detectan
  mediante un recorrido iterativo.
- Sus hijos se procesan en postorden y de izquierda a derecha mediante un `std::vector` alojado en heap.
- Los resultados conservan las cuatro opciones semánticas de la solicitud: grupos de métodos, eventos,
  movimientos y escritura de propiedades desde constructores.
- Las expresiones cortas y las construcciones con flujo especial conservan el camino anterior.
- No se aumentó la pila, no se ocultó el error y no se modificó el código de la biblioteca.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`: estados del worklist y contratos auxiliares.
- `src/Frontend/ProjectMethodBodyModel.cpp`: coordinador iterativo y separación del núcleo semántico.
- `tests/unit/ProjectMethodBodyModelTests.cpp`: regresión con 40 niveles de llamadas y sumas, generada mediante
  un bucle iterativo.

No se creó ningún archivo nuevo de producto. Este documento es la evidencia de la tanda.

## Verificación dirigida

- Build exclusivo del target `nsc`: correcto.
- Contrato de runtime estático del ejecutable: correcto.
- Target exclusivo `nsc_project_method_body_model_tests`: `PASS: 60 checks`.
- Regresión nueva: una expresión de 40 niveles se analiza sin agotar la pila del host.
- `NativeSharp.Math.Functions`, O0: correcto. `method-bodies` terminó en 2,339,082 ms y `construction` en
  409,282 ms; el proceso no volvió a producir `0xC00000FD`.
- Artefacto COFF AMD64: `NativeSharp.Math.Functions.lib`, 25,437,566 bytes, SHA-256
  `29B285B2DF35DD0B554D86275F40A2DF2E9460001BE57D7AF0329AA1B31158FA`.
- Metadata: `NativeSharp.Math.Functions.nsmeta`, 9,893,332 bytes, SHA-256
  `D9255448B868450EF6016444AA85266529A01D089162016B30178CB3E4CD7F78`.
- Header: `NativeSharp.Math.Functions.h`, 1,771 bytes, SHA-256
  `E47490EBF4648311CDFF8F19C996F14BBDC7A89D0A0CC90715E2225FB9A13C13`.

No se ejecutó baseline, CTest completo, matriz de optimizaciones ni compilación general de bibliotecas.
