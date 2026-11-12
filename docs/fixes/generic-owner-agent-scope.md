# Alcance del agente 2: método genérico de propietario genérico

- Objetivo: investigar y corregir el caso 70-method del informe y su consumo
  binario, conservando la reproducción original y sin alterar préstamos.
- Base local inicial: fc06f5a38, descendiente de a56498f05; el árbol principal
  integró los cambios de decimal por commits propios después de esa base.
- Naturaleza: corrección de especialización y superficie binaria NativeSharp;
  preservar las reglas de identificación, restricciones, caché y seguridad.
- Material de partida: ensayo sin registrar en A:/nsc-devel-fixes, sujeto a
  revisión y traslado selectivo; no se modifica esa copia.
- Archivos candidatos: ProjectGenericModel, archivos de Generics y producción
  de superficie precompilada que participen en la reproducción, pruebas
  dirigidas existentes, su registro y configuración mínima de pruebas.
- Protegidos: propiedad/préstamos salvo dependencia demostrada y coordinada;
  C/C++ interop, Daikon y LLVM/Clang externos; A:/nsc-devel y C:/native-sharp.
- Builds, resultados, temporales y cachés en A y separados; compilación
  incremental y pruebas dirigidas únicamente. Sin push, merge ni despliegue.
- La evidencia distinguirá ejecutable híbrido y compilador integrado. La
  corrección solo se entrega tras validación dirigida y commits revisables.

## Ampliación para artefactos históricos

- Reproducción: biblioteca generada el 10/11 con `Box` y `Box<N>`; al
  consumir `Wrapper.nested.Echo` se reconstruyen dos métodos en la clase
  ordinaria y se rechaza una familia duplicada en ocho cruces O0/O2.
- Ruta causal: `PrecompiledSurfaceReachability::includeTemplate` alcanza el
  dueño por su nombre sin aridad y deja fuera la plantilla `Box<N>` aunque
  mantiene sus métodos, cuya identidad completa sí consta en la metadata.
- Archivos candidatos: `src/Compilation/PrecompiledSurfaceReachability.cpp`,
  prueba dirigida de clausura y su registro. Se protege el resto del driver,
  análisis de préstamos y el formato de metadata.
- Comportamiento requerido: alcanzar el dueño exacto indicado por la clave de
  método y conservar propietarios ordinarios homónimos, sin adivinar claves.

## Ampliación para receptor indexado

- `Lookup.this[int]` devuelve `Own<1>`; `lookup[0].At<20, 42>()` rechaza
  `packat(Ns, N)` antes de sustituir `N`, en O0/O2.
- Candidato: selección de dueño en `GenericCallInference.cpp`; conservar
  análisis de préstamos, semántica de indexadores y resolución de sobrecargas.
- La variante en estructura reproduce el mismo fallo: su contexto léxico no
  se registra. Candidato adicional: registro de contexto de tipo en
  `TemplateDependencyCollector.cpp` para la declaración de estructura.
- Inferir el resultado solo cuando se identifique inequívocamente la declaración
  y su indexador aplicable; no convertir una suposición en resolución general.
- El mismo fallo se reprodujo en contenedores genéricos, sobrecargas `int` y
  `long` y bases ordinarias: la resolución utiliza sustituciones del dueño,
  coincide tipos primitivos exactos y sigue una base identificada de forma
  única, sin reemplazar la resolución semántica de indexadores.
- Verificar positivos y negativos en O0/O2 y registrar cualquier forma aún no
  demostrada, antes de incorporar cambios de mayor alcance.

## Ampliación para tipos nominales y conversiones

- En O0/O2, guardar el resultado de un indexador y luego invocar el método
  genérico devuelve 42 tanto con `Key` como con conversión de `int` a `long`;
  la llamada directa falla. Evidencia en
  `A:/nsc-fixes-agente-2-evidencia/generic-indexer-conversions-before/results.json`.
- Candidato: comparación de sobrecargas en `GenericCallInference.cpp`;
  reutilizar clasificación numérica existente e identidades nominales
  resueltas, sin cambiar la resolución semántica general ni préstamos.
- Preservar diagnósticos para ambigüedades y no escoger un candidato cuando
  falte información suficiente. Validar fuente, biblioteca y ámbito léxico.

## Const local y diagnóstico ambiguo

- La lectura semántica de un local o parámetro no adjunta `integerConstant`,
  incluso si se declara `const`. En la reproducción, el control ordinario
  selecciona `long` sin usar el valor; la llamada directa aún falla.
- Candidato: `GenericCallInference.cpp`, al clasificar una expresión
  identificadora cuyo binding está disponible en el ámbito. No calcular ni
  inventar el valor del local.
- Para ambigüedad, emitir un diagnóstico de indexador solo cuando todos los
  candidatos numéricos se hayan clasificado y no exista uno dominante;
  conservar la ruta anterior ante información o reglas adicionales no probadas.

## Accesibilidad de indexador privado

- Base de trabajo: `56c861ab5`. En fuente, un getter público `short` y otro
  privado `ushort` causan rechazo de `items[byteIndex].At<42, 0>()` fuera del
  dueño; el acceso ordinario devuelve 42. En biblioteca el privado no se exporta.
- Candidato causal: recolección de candidatos de indexador en
  `GenericCallInference.cpp`. Excluir un privado solo cuando se demuestre que
  el tipo que accede es diferente al declarador, sin cambiar la semántica
  general ni el acceso desde el propio dueño. Ante identidad desconocida,
  conservar el comportamiento previo.
- Archivos candidatos: ese archivo, `tests/integration/generic_owner_indexer.py`
  y documentación de evidencia. Validar acceso propio, externo, tipos
  homónimos, fuente y bibliotecas, y rechazo de getter privado inaccesible.
- Siguen protegidos préstamos, interoperabilidad C/C++, Daikon, fuentes
  externas, la copia principal y sus builds; solo build incremental y pruebas
  dirigidas en rutas independientes de A. Sin push, merge ni despliegue.

## Receptor this indexado

- Base: `07319020e`. Dentro de `PrivateCandidate`, `this[index].At<0, 42>()`
  falla antes de encontrar el método genérico, aunque almacenar `this` en un
  local de tipo `PrivateCandidate` y llamar sobre `local[index]` funciona.
- El punto causal es el tipo de la colección en `processGenericCall`: la
  inferencia de expresiones no da un tipo para `ThisExpression`. Candidato:
  resolver el dueño léxico exacto únicamente en esta ruta de indexador; no
  modificar inferencia global ni tratar el nombre simple como identidad.
- Reutilizar pruebas de fuente, bibliotecas, visibilidad y regresiones del
  indexador. Mantener restricciones y archivos protegidos indicados arriba.

## This indexado en dueño genérico cerrado

- Base `4c131f94c`: dentro de `GenericLookup<1>`, `this[0].At<20, 42>()`
  no encuentra la plantilla aunque el indexador sobre una variable externa de
  `GenericLookup<1>` devuelve 42. Evidencia previa en
  `A:/nsc-fixes-agente-2-evidencia/generic-indexer-this-generic-before.log`.
- Se investigará el clon especializado ya registrado y su contexto de
  declaración, sin reconstruir argumentos por el nombre ni cambiar el acceso.
- El log anterior `generic-indexer-this-fix/generic_owner_indexer.log` fue
  sobrescrito por error al reproducir este caso; el coordinador conserva y
  ejecutará la validación integrada del commit anterior. Los siguientes logs
  se guardarán bajo rutas nuevas.

## Clausura de metadata y this indexado

- Base `819a1312b`. Un productor público genérico con llamada
  `this[0].At<...>()` se publica si `At` es único globalmente; agregar un
  método genérico homónimo en otro tipo o namespace causa rechazo de clausura.
  Cambiar solo el nombre del método adicional restaura la publicación.
  Reproducciones en `generic-indexer-closure-isolation/` de la evidencia.
- Candidato causal: `collectCallDependency` no identifica el dueño resultante
  de `this[index]` y recurre a la búsqueda global. Considerar únicamente un
  indexador propio único y tipo genérico de retorno resuelto, conservando la
  vía existente si falta información o existe más de un indexador.
- Archivo candidato `TemplateDependencyCollector.cpp`, pruebas y registro;
  no cambiar el formato de metadata, accesibilidad, interoperabilidad ni
  análisis de préstamos. Validar productores mínimo y amplio y consumidores.

## Clausura con indexadores sobrecargados

- Base `12c3975b9`: indexadores `int` y `long` de `GenericLookup<N>` devuelven
  `Fixture.Own<N>`; con un `At` homónimo en otro tipo, publicar la biblioteca
  falla en O0/O2. Registro `indexer-overload-closure-before/results.log` en
  la carpeta de evidencia independiente.
- Candidato: reconocer que todos los getters de aridad pertinente devuelven la
  misma declaración genérica exacta; registrar sus métodos, sin seleccionar
  indexador. Si un getter no se conoce o devuelve otro tipo, conservar la vía
  anterior. No cambiar resolución semántica ni metadata física.

## Clausura con retornos genéricos distintos

- Base `e8d9f0189`: dos getters devuelven `Fixture.Own<N>` y `Wrong.Own<N>`;
  dos métodos escogen respectivamente `int` y `long` y ejecutan 42 juntos al
  compilar desde fuentes O0/O2, pero publicar falla en ambos modos. Evidencia:
  `A:/nsc-fixes-agente-2-evidencia/indexer-distinct-closure-before/results.json`.
- Candidato: conservar las dependencias de todos los dueños de retorno
  inequívocamente resueltos cuando todos tienen métodos correspondientes; no
  decidir ni reescribir la sobrecarga. Si alguno es desconocido, tiene bases o
  carece de método, conservar la vía anterior y evitar una selección inventada.
- Mantener los demás límites y archivos protegidos arriba; validar fuente,
  consumidores estáticos y DLL, homónimos y ausencia de método aplicable.

## Campo de receptor indexado

- Base `081032d60`: `items[0].Value.At<20, 42>()` se rechaza en O0/O2;
  guardar `items[0]` como `Holder<1>` antes de llamar al mismo campo ejecuta 42.
  Registro `A:/nsc-fixes-agente-2-evidencia/indexer-field-receiver-before/results.json`.
- Candidato: rama de inferencia de campo en `GenericCallInference.cpp`, que
  reconoce la indexación de arreglos pero no la de un getter de usuario.
  Evaluar únicamente dueño ordinario sin bases y getter público único, usando
  la unidad, imports y aliases de la declaración del getter para su retorno.
- No adivinar tipos ni modificar préstamos, binding general o interoperabilidad;
  controles de identidades homónimas, metadata binaria y campos inválidos.

## Campo de indexador sobre dueño genérico

- Base `7fa97ff72`: `GenericLookup<1>[0].Value.At<20,42>()` falla en
  O0/O2; guardar el resultado `Holder<1>` primero ejecuta 42. Evidencia
  `A:/nsc-fixes-agente-2-evidencia/generic-indexer-field-before/results.json`.
- Candidato: la misma rama de inferencia de campo de
  `GenericCallInference.cpp`, con dueño genérico único y cerrado y getter
  público único. Sustituir argumentos de la plantilla con el mecanismo
  existente; respetar unidad y ámbito de declaración.
- Controles: distintas instancias, identidad de tipos, campo privado y consumo
  desde bibliotecas. No tocar bindings generales, préstamos ni interoperabilidad.
