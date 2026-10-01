# Tanda correctiva G00: paridad frontend SH42 de NativeSharp 3.0

## Problema observado

El baseline llegó a `nsc.selfhost.functional-equivalence` y falló al comparar el volcado de
tokens del compilador principal (`nsc0`) contra el compilador autoalojado (`nsc2`) en
`tests/cc1/c55_ns3_calls_initializers/Main.ns`.

La primera diferencia real estaba en `..basis`: nsc0 emitía `DotDot`, mientras el lexer
autoalojado todavía emitía dos tokens `Dot`. Al retirar ese primer bloqueo, la prueba dirigida
reveló otras dos divergencias que el verificador de parada temprana no había podido mostrar:

- `#run(20 + 1)` no se evaluaba antes de lexear en el autoalojado;
- las llamadas con argumentos genéricos explícitos perdían `flag=true` en su AST.

## Cambios

### Lexer autoalojado 3.0

- Se agregaron al final de `TokenKind` los cinco tokens 3.0 para preservar todos los valores
  numéricos 2.x: `InterpolatedStringLiteral`, `QuestionQuestion`,
  `QuestionQuestionEqual`, `QuestionDot` y `DotDot`.
- Se aplicó maximal munch a `??=`, `??`, `?.`, `...` y `..`.
- Se sincronizó el reconocimiento de cadenas interpoladas `$"..."`.
- Se sincronizó el reconocimiento de cadenas raw con delimitador de tres o más comillas.

### Preprocesamiento compile-time

- Se creó `selfhost/nsc/Source/ComptimeProcessor.ns`.
- Las rutas frontend cargan las fuentes mediante `SourceBuffer.LoadFrontend`.
- `#run(...)` se evalúa antes del lexer y el resultado se rellena hasta conservar exactamente
  el ancho original. Por ello los offsets, rangos y diagnósticos siguen alineados con nsc0.
- El lector crudo usado por `--read-source`/`--dump-source-info` permanece sin transformación.

### AST de llamadas genéricas

- `Parser.ParsePostfix` conserva si una llamada escribió argumentos genéricos explícitos y
  publica ese hecho mediante `CallExpression.flag`, igual que nsc0.
- Esto distingue correctamente `F<>()` de `F()` incluso cuando la lista de tipos es vacía.

## Archivos modificados

- `selfhost/nsc/Lexer/TokenKind.ns`
- `selfhost/nsc/Lexer/Lexer.ns`
- `selfhost/nsc/Source/SourceBuffer.ns`
- `selfhost/nsc/Parser/Parser.ns`
- `selfhost/nsc/Driver/Program.ns`
- `selfhost/nsc/Bootstrap/GenerationBuilder.ns`
- `selfhost/nsc/Sema/ModuleSet.ns`

## Archivo nuevo y motivo

- `selfhost/nsc/Source/ComptimeProcessor.ns`: concentra la transformación previa al frontend y
  evita duplicar reglas de `#run` en el driver, el bootstrap y la compilación multiunidad.

## Mejora obtenida

El compilador autoalojado vuelve a consumir la superficie 3.0 con el mismo contrato observable
que nsc0. La corrección no reduce el corpus ni agrega exclusiones a SH42: elimina las diferencias
en su origen.

## Validación dirigida

- Compilación del candidato autoalojado O0 desde las fuentes actuales: correcta.
- Paridad exacta de 709 tokens para `c55_ns3_calls_initializers/Main.ns`: correcta.
- Paridad exacta de 27 tokens para `ns3_t11_comptime/Main.ns`: correcta.
- Paridad exacta de 229 líneas AST para `c44_constraints_genericos/Main.ns`: correcta.
- Verificador frontend SH05/SH42 dirigido: 588 fuentes, 1,176 comparaciones de tokens,
  32 comparaciones AST y 10 casos de recuperación, todos correctos.
- No se ejecutó el baseline completo.
- `C:\native-sharp` no fue leído, copiado ni modificado.
