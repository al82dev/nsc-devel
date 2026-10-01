# Tanda correctiva G00 — semántica dirigida después del baseline cancelado

**Fecha:** 1 de octubre de 2026  
**Rama:** `development`  
**Alcance:** compilador NativeSharp 3.0; sin baseline, librerías externas, instalación ni deploy

## Motivo

La corrida G00 cancelada dejó seis rojos funcionales pequeños después de separar los fallos de
identidad, NSMETA y MIR ya corregidos en tandas anteriores. Esta tanda reproduce cada rojo con su
binario individual y corrige dos defectos reales, tres diagnósticos/contratos desalineados y dos
expectativas que todavía describían capacidades anteriores a las tandas 3.0.

## Cambios

### Asignación definida y diagnósticos

- `ref x` e `in x` ya no atraviesan el diagnóstico reservado para el operador de dirección crudo
  `&x`. Siguen siendo lecturas y ahora informan `use of unassigned local variable`, mientras que
  un `&x` auténtico conserva la explicación de puntero crudo.
- Un inicializador posicional con demasiados valores vuelve a informar ambas cardinalidades, por
  ejemplo `has 2 values but the type takes 1`. Los inicializadores generalizados conservan el
  mensaje cualitativo porque un spread o un nombre no equivalen a una cardinalidad posicional.
- Los parámetros genéricos duplicados distinguen `generic type parameter` de `const generic
  parameter`, en lugar de perder la clase de parámetro en un diagnóstico genérico.

### Funciones libres como llamadas y como valores

- El recorrido de `ProjectNamespaceFunctionModel` marca el callee directo de una llamada. La
  `CallExpression` recibe un enlace de llamada y su `IdentifierExpression`/`MemberExpression` no
  vuelve a registrarse como valor callable.
- Los usos reales como valor —delegados y argumentos target-typed— siguen recibiendo su enlace.
  El receptor del callee también continúa recorriéndose, de modo que no se pierden referencias a
  estado de namespace u objetos.

### Contratos 3.0 actualizados

- El lexer ya reserva `..` para rangos de edición 3; la prueba de variádicos ahora verifica la
  convivencia correcta de `.`, `..` y `...` mediante maximal munch.
- Runtime interpolation fue implementada por T29. En un proyecto deliberadamente aislado de Core,
  la prueba ahora congela la dependencia explícita ausente `String` en vez del rechazo histórico
  que afirmaba que todavía faltaba diseñar un handler.
- T41 declaró que los parámetros legacy `string` sin calificador son non-retaining. La prueba de
  borrow valida tanto `borrowed string` explícito como esa superficie legacy, y exige que ambos
  registren el owner temporal oculto de expresión completa.

## Archivos modificados

- `src/Frontend/ProjectMethodBodyModel.cpp`
- `src/Frontend/ProjectNamespaceFunctionModel.cpp`
- `src/Parser/Parser.cpp`
- `tests/unit/Cc1CorrectnessTests.cpp`
- `tests/unit/Item331VariadicCDeclarationTests.cpp`
- `tests/unit/Stage50_4BorrowedParametersTests.cpp`
- `docs/compiler-optimization/TANDA-CORRECTIVA-G00-SEMANTICA-DIRIGIDA.md` — expediente nuevo de la
  tanda, requerido para conservar causa, archivos, mejora y evidencia junto al commit.

## Validación dirigida

Se construyó `nsc` y se compilaron únicamente los binarios afectados. Resultados:

- Stage 56 C7 definite assignment: **77/77**.
- Stage 44 generics monomorphization: **81/81**.
- Item 331 variadic declarations: **18/18**.
- Stage 50.4 borrowed parameters: **25/25**.
- Namespace function lookup: **9/9**.
- CC1 `--rest`, la partición que contenía el rojo: **496/496**.
- T08 callable values: **9/9**.
- Namespace function model: **7/7**.
- CC1 fixture C66 callable values: **6/6**.
- Stage 45 linker/PE/EH, verificada antes de editar esta tanda: **32/32**.

Total documentado en esta tanda: **760 comprobaciones dirigidas correctas**. Los temporales creados bajo
`A:\nsc-devel\build-test-lldlinker` fueron eliminados. No se ejecutó baseline, CTest general,
O1/O2/O3, compilación de librerías, instalación, deploy ni ninguna operación sobre `C:`.

## Mejora concreta

La próxima corrida deja de fallar por contratos obsoletos y, sobre todo, evita que una sola llamada
libre contamine el modelo con dos enlaces distintos. Los diagnósticos de `ref`, agregados y
genéricos vuelven a describir la sintaxis escrita, mientras las capacidades 3.0 de rangos,
interpolación y temporales string quedan probadas según su contrato vigente.
