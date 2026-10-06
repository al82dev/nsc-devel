# SEM-01.02: indice de tipos de expresion integrado

**Requisito:** `SEM-PERF-001`

**Tag:** `NSC.SEMANTICS.INDEX`

## Cambio

`ProjectMethodBodyModel` integra una instancia de `ProjectCallableBindingIndex` para sus tipos de expresion. Cada
`MethodExpressionTypeBinding` se agrega primero al vector determinista y su ordinal se publica despues bajo la clave
`(syntax, methodMemberOrdinal)`.

La nueva consulta `expressionTypeBinding` devuelve un puntero estable mientras el modelo sea inmutable. El vector
continua siendo la fuente de orden para dumps, metadata y consumidores que recorren todos los bindings. La consulta
indexada no cambia ni reordena ese contrato.

## Invariantes

- la clave incluye el callable y no confunde materializaciones que comparten sintaxis;
- una anotacion posterior para la misma clave reemplaza solamente el ordinal indexado;
- el indice guarda ordinales y resiste realocaciones del vector;
- una consulta ausente devuelve `nullptr` sin recorrer el vector;
- `build` limpia el indice antes de producir bindings;
- no se introduce recursion.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`;
- `src/Frontend/ProjectMethodBodyModel.cpp`;
- `tests/unit/ProjectMethodBodyModelTests.cpp`.

## Validacion puntual O0

Se construye solamente `nsc_project_method_body_model_tests` y se ejecuta directamente con `TEMP`, `TMP`, evidencia
y artefactos bajo `A:`. La prueba existente conserva sus contratos y agrega una comprobacion de que la ultima
anotacion publicada se recupera por sintaxis y callable exactos.

No se ejecuta baseline, minibaseline, CTest general, O1, O2 ni O3.

## Evidencia

- build dirigido: correcto;
- ejecucion directa: `PASS: 62 checks`;
- el analisis estatico dirigido alcanzo los archivos modificados y conservo su reporte en
  `_nscbuilder/semantic-performance/SEM-01.02/clang-tidy.txt`;
- la pasada no se declara limpia: el `clang-tidy` disponible no resolvio `<string_view>` ni `<cstdint>` bajo el
  comando MSVC del build y tambien enumero warnings preexistentes del archivo monolitico;
- la compilacion con warnings como errores no produjo un hallazgo nuevo asociado al indice.
