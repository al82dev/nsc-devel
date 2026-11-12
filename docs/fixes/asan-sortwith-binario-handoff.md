# Caso 100: inferencia de `SortWith` con Collections binario

## Alcance y estado

La reproducción original del reporte se conserva intacta en
`A:/native-sharp-reference/code-examples/chapter-23/100_stl_algorithm_and_spans/expected-errors/sort_with/Program.ns`.
En su línea 23 llama `Algorithm.SortWith(Spans.Of(values), order)`.
El SDK de esta rama sí declara `SortWith<T>(Span<T>, Comparison<T>)` en
`lib/NativeSharp.Core.Collections.Generic/Algorithm.ns:303`; su cuerpo delega
en `Sort<T>`. El fallo observado es la inferencia al consumir Collections como
referencia binaria, no la ausencia actual del alias en el código o metadata.

Estado: **reproducido, causa por confirmar y corrección pendiente**. Este
handoff es de investigación: no modifica el motor de genéricos ni cambia el
ejemplo. Las restricciones completas están en `TAREAS-CHAT-ASAN.md` y
`A:/nsc-asan-evidencia/SCOPE-LOCK.md`. El frente de genéricos y fases del
compilador pertenece a `A:/nsc-limitaciones-campos`; coordinar con ese chat y
con el integrador antes de editar cualquier archivo compartido.

## Evidencia reproducible

- `A:/nsc-asan-sdk-evidencia/test-collections-results.json` conserva las
  invocaciones, configuración, diagnósticos y hashes del ensayo binario.
  El compilador propio tiene SHA-256
  `49b8952fb6e0676b5ab35ca09b19df888e64a332ec3fe88cbcc04d81a868a47d`.
- La copia byte por byte del original, consumiendo Core y Collections por
  `LibraryReference`, fue rechazada en O0 y O2 tanto sin ASan como con ASan,
  con `type arguments for generic method 'SortWith' cannot be inferred`.
  El error se produce antes de generar o ejecutar código instrumentado.
- `A:/nsc-asan-sdk-evidencia/sortwith-controls.json` y su `.log` contienen los
  controles: el mismo original con `ProjectReference` a Collections fuente
  compiló y ejecutó en O0/O2; una copia distinta que escribe `SortWith<int>`
  también compiló y ejecutó con Collections binario. La copia es un control,
  no una modificación ni sustitución de la reproducción original.
- Los controles de `sortwith-local-explicit-of-controls.json` inicializan el
  local con `Spans.Of<int>(values)` para aislar la segunda llamada. Con local
  `Span<int>`, `SortWith(local, order)` pasa el análisis por fuente y falla por
  binario; con `NativeSharpCore::NativeSharp.Core.Span<int>`, falla por fuente
  y pasa por binario. Con `NativeSharp.Core.Span<int>` falla en ambos. Los
  comandos y diagnósticos están en ese JSON; las variantes con `Of` inferido
  están en `sortwith-local-controls.json` y no aíslan la misma operación porque
  las formas cualificadas ya fallan al inferir `Of`.
- `tests/integration/sort_with_alias.py` cubre `SortWith` por
  `ProjectReference`; no verifica esta llamada mediante `LibraryReference`.
- El volcado `A:/nsc-asan-sdk-evidencia/collections-api.log` contiene la
  plantilla del alias. Su extracción legible y comando están en
  `extract-sortwith-api.py` y `dump-collections-api.cmd` en la misma carpeta.
  La metadata de Collections usada tiene SHA-256
  `76ac20d3d01504fce6b80cd10c2b5403f7857f1034bfc5ae8bec3e2ceaa401f1`.

## Traza estática e hipótesis no demostrada

La representación serializada de la plantilla `SortWith` en Collections
escribe el primer parámetro como
`NativeSharpCore::NativeSharp.Core.Span<T>`. La plantilla `Spans.Of<T>` en la
metadata de Core escribe su retorno como `Span<T>`. En el código fuente ambos
se escriben con `Span<T>`.

1. `src/Driver/PrecompiledSurface/PrecompiledSourceGeneration.cpp:110`
   reconstruye las declaraciones genéricas desde `sourceText` de metadata.
2. `src/Frontend/Generics/GenericCallInference.cpp:207` intenta inferir la
   llamada anidada primero; `:529` clona y registra el tipo de retorno.
3. `src/Frontend/Generics/TypeAliasExpander.cpp:94` muestra que
   `cloneAliasType` conserva `baseName` salvo sustitución de parámetro.
4. `src/Frontend/Generics/GenericTypeInference.cpp:259` recupera el retorno
   inferido de la llamada. `:486` compara patrones y argumentos; en `:510`
   exige igualdad textual de `baseName`, además de clase y aridad.

**Hipótesis:** un tipo nominal equivalente llega escrito de forma cualificada
en el patrón y sin cualificar en el resultado de `Spans.Of`, por lo que la
comparación textual rechaza inferir `T`. Los controles por grafía y la traza
estática son evidencia fuerte a favor de una discrepancia de representación,
pero **no se observaron los objetos `TypeSyntax` efectivos durante el fallo**;
no está demostrada la función exacta que origina la divergencia. No debe
aplicarse una normalización textual indiscriminada: podría confundir tipos
homónimos de módulos o namespaces distintos y alterar resolución o accesibilidad.

La traza de patrón y argumento ya existe en
`GenericTypeInference.cpp:495–500`, pero está dentro de `#if 0`; el parser de
`--generics-trace` también está deshabilitado. Por tanto la configuración
actual no ofrece esa observación directa mediante dicha opción. La lectura
realizada no modificó ni recompiló el frontend.

## Siguiente prueba y fronteras

Para confirmar la cadena, el responsable de genéricos puede habilitar
instrumentación acotada en un build propio y registrar ambos `baseName` y sus
argumentos en la comparación fallida. Debe incluir controles de identidades
equivalentes, tipos homónimos distintos, ambigüedad y argumentos explícitos
antes de proponer una corrección semántica. Mantener los controles actuales y
ejecutar la reproducción original sin modificar en O0/O2 tras la corrección.

El fragmento diagnóstico **no aplicado ni compilado** está en
`A:/nsc-asan-evidencia/sortwith-diagnostico-no-aplicado.md`. Especifica punto
de inserción, datos impresos, controles, límites y retiro; no cambia la decisión
del compilador. Su compilabilidad y la traza real permanecen sin verificar.
`include/nativesharp/AST/TypeSyntax.h:161` documenta un fallo anterior causado
por modificar `baseName` y una separación de `originNamespace`: esto refuerza
la necesidad de conservar identidad y contexto, no eliminar prefijos por texto.

Archivos potencialmente compartidos: `GenericCallInference.cpp`,
`GenericTypeInference.cpp`, `TypeAliasExpander.cpp`,
`PrecompiledSourceGeneration.cpp` y pruebas de inferencia/metadata. Identificar
la causa antes de seleccionar cuáles cambiar; este hallazgo no autoriza editar
todos. No hay cambios de producto para este caso en la rama de ASan.
