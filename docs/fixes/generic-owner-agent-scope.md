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
