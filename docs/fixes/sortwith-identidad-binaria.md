# Inferencia de SortWith con SDK binario

El ejemplo original del caso 100 llamaba `SortWith(Spans.Of(values), order)`.
Con Core y Collections binarios se rechazaba la inferencia en O0/O2. La traza
`A:/nsc-fixes-evidencia/sortwith-trace-context-integrado.log` mostró el patrón
`NativeSharpCore::NativeSharp.Core.Span<T>` frente al retorno `Span<int>`.
La comparación exigía igualdad textual antes de examinar el argumento `T`.

`recordGenericInferenceReturnType` conserva la plantilla resuelta en el
contexto de declaración del retorno de una única candidata. Al comparar un
patrón cualificado por módulo, `inferTypeArguments` sigue la cadena de clones
y acepta nombres distintos solo si ambas referencias resuelven a la misma
plantilla. Una resolución ausente, ambigua o distinta no prueba igualdad.
No se eliminan prefijos textualmente ni se modifica el formato de metadata.

Con compilador reconstruido incrementalmente y Core/Collections aislados
producidos en O0 y O2, el original se compiló y ejecutó en ambos niveles de
consumidor para cada par de bibliotecas. El control de semántica también pasó
para llamadas inferidas y explícitas, y rechazó una plantilla `Other.Span<T>`
homónima. El control de Collections desde fuente pasó en O0/O2. Comandos,
hashes y resultados se encuentran en `A:/nsc-fixes-evidencia/final-*.log`:
`trace-sortwith-integrado-O0`, `trace-sortwith-integrado-O2`,
`validar-sortwith-semantica-O0`, `validar-sortwith-semantica-O2` y
`validar-sortwith-fuente.cmd`.

El ajuste cubre el retorno inferido de una única candidata; no acredita los
casos con varias candidatas ni todas las formas de variables o alias. Las
bibliotecas usadas se generaron antes de este cambio, que solo afecta al
compilador. No se ejecutó una batería completa ni se modificó el ejemplo.
