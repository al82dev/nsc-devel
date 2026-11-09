# Decisiones de diseño pendientes

Revisión observada: `ab5870aa8a97008602c2d4d8631ea3c277d7284b` en
`A:/nsc-devel-fixes`. Las reproducciones originales y sus resultados se
conservan en `cases`; ninguna propuesta de este archivo está implementada por
el hecho de aparecer aquí. Las preguntas ya enviadas están pendientes de
respuesta mientras se continúa la validación independiente, salvo las
decisiones posteriores indicadas explícitamente abajo.

## checked sobre i128

- Evidencia: `cases/comp-03/result.json` muestra que la suma de uno al máximo
  termina normalmente con wrapping; el evaluador de políticas rechaza el
  overflow checked en `src/Language/NumericSemantics.cpp`.
- Decisión adoptada: el usuario eligió trap de overflow. La suma firmada i128
  se corrigió en `93d859e31`; otras operaciones no se han acreditado. Véase
  `checked-i128-add.md`.
- Antecedente cercano: el backend usa una trampa específica para overflow de
  `decimal` (`LlvmTrapReason::DecimalOverflow`), pero no se ha demostrado que
  ese código o contrato corresponda a overflow entero `i128`.
- Consecuencia: el código de fallo es observable y debe probarse para O0/O2,
  valores normales y límites antes de declarar corregido el caso.

## Cancelación y limpieza async

- Evidencia: `cases/comp-06/result.json` registra traza 0 y salida 5 después
  de `Cancel()`. `cases/comp-06-deferred/result.json` registra traza 7 y salida
  0 cuando se añade el siguiente `MoveNextAsync()`. La copia de diagnóstico es
  distinta del original y no sustituye la reproducción.
- El documento `docs/3.0.4/items/NS304-T07-F05-cancellation.md` describe la
  cancelación cooperativa y el cleanup en el siguiente avance, además de
  `Drop()` para abandono. El usuario confirmó la limpieza al siguiente avance;
  la reproducción con ese avance pasa. No se implementó limpieza inmediata.

## Punteros compartidos

- Evidencia: `cases/sdk-01` no resuelve Rc, WeakRc, Arc ni WeakArc. El
  manifiesto de `src/Language/SmartPointerSemantics.cpp` describe familias,
  operaciones, destrucción, módulo y concurrencia; `lib/NativeSharp.Core.Threading/ReferenceCounting.ns`
  implementa únicamente estado de contadores y hooks, no esas familias.
- Decisiones adoptadas: el usuario autorizó las cuatro familias para clases y
  estructuras, y una fábrica adicional que propague excepciones y libere el
  bloque si falla. Confirmó también que `Arc.WeakCount()` será aproximado
  durante operaciones concurrentes y exacto en ausencia de ellas, y que
  `Rc.Get()` y `Arc.Get()` devolverán una referencia prestada de solo lectura
  vinculada al propietario, incluso para estructuras. Existe una primera
  implementación pública en el árbol de trabajo, aún sin commit y sin
  validación de consumo binario. Las variantes internas de excepción rechazan
  por ahora formas de payload cuya limpieza no está acreditada; la vida del
  módulo, la seguridad de alias y la transferencia entre hilos siguen abiertas.
  Véanse `diseno-punteros-y-asan.md` y `prestamo-get-punteros.md`.
- Al probar la API pública atómica desde un método de hilo, una variante con
  `WeakRc` almacenado en un campo estático fue aceptada semánticamente, aunque
  `WeakRc` es local al hilo. No se ejecutó la variante con contadores locales.
  La evidencia está en `public-local-thread-negative.log`. El usuario eligió
  prohibir almacenamiento estático compartido de `Rc`/`WeakRc`, incluidos
  contenedores; esa medida conservadora también rechazará casos estáticos
  usados por un solo hilo. La validación y sus límites para módulos
  precompilados están documentados en `shared-static-storage.md`.

## impl para struct y métodos genéricos en tipos genéricos

- Evidencia: `cases/68-struct/build.log` exige como destino una clase no
  estática local. `cases/70-method/build.log` rechaza expresamente métodos
  genéricos dentro de tipos genéricos; la función externa corregida en la
  campaña anterior no implementa ese contrato.
- Decisión adoptada: admitir `impl` externo para estructuras con despacho
  estático y sin boxing; implementado en `f3b0314c0` con regresión. Sigue
  pendiente decidir por separado la especialización combinada del tipo y del
  método, incluidos límites y errores.

## Restricciones entre parámetros genéricos

- Evidencia: `cases/69-constraint` reproduce `unknown type name 'T'` dentro
  de `Reducer<T>`. El comentario en
  `src/Frontend/NameLookup/SyntaxBinding.cpp` supone que otra validación no
  permite referencias a otros parámetros, aunque esta forma anidada llega al
  resolvedor. La muestra del reporte fue reconstruida.
- Decisión: especificar si un constraint de un parámetro puede depender de
  otro y cómo se comprueba después de especializar. No basta omitir el error de
  nombres: debe existir una comprobación de satisfacción del tipo construido.
- Control adicional: una restricción cerrada `TOp : Reducer<int>` acepta una
  implementación `Reducer<int>` y su programa termina con 42 en O0/O2; la
  restricción `Reducer<long>` rechaza esa misma implementación en ambas
  emisiones. Comandos y diagnósticos: `generic-trait-concrete-control/results.json`
  y `generic-trait-mismatch-control/results.json` en `A:/nsc-fixes-evidencia`.
  Esto demuestra la distinción de dos tipos cerrados, no el soporte para
  `Reducer<T>` ni la sustitución de parámetros entre restricciones. Se consultó
  al usuario si debe admitirse la relación dependiente con verificación después
  de especializar; sigue pendiente su respuesta.
  En el código actual `cloneGenericConstraints` clona cada tipo de restricción
  con el mapa de sustituciones y `appendTypeConstraintViolation` verifica la
  relación sobre tipos ya resueltos. `bindGenericTemplateConstraints`, en cambio,
  intenta resolver también los argumentos anidados de la declaración abierta y
  supone que no pueden nombrar otro parámetro. Estas observaciones identifican
  partes de la ruta, pero no prueban que corregir solo nombres sea suficiente.

## Temporales en parámetros in y operadores ++/--

- Evidencia: `cases/70-temporary` rechaza el temporal; `cases/39-increment`
  explica que la sobrecarga podría evaluar dos veces el operando al almacenar
  el resultado. El caso decremento también es rechazado.
- Reproducción actual: el análisis con el compilador aislado sigue rechazando
  `value == new Rational(1, 1)` sin modificar los archivos originales
  (`A:/nsc-fixes-evidencia/temporary-in-current-result.json`). En
  `ProjectMethodBodyModel::evaluateSyntheticCandidates`, la condición
  `implicitOperatorReference && !addressable` descarta el operando temporal;
  esa observación no demuestra por sí sola que las fases posteriores puedan
  materializarlo ni que su vida y destrucción sean correctas. En HIR,
  `boundCallArgumentNeedsAddress` solicita dirección para todo parámetro que
  no es por valor, de modo que quitar solo el rechazo semántico no acredita una
  solución completa. La propuesta consultada conserva el temporal hasta el
  fin de la expresión, evalúa una sola vez y prohíbe el escape de la referencia.
- Decisión: definir si el temporal se materializa y por cuánto tiempo vive;
  por separado, si se admiten `++`/`--` definidos por el usuario y cómo se
  garantiza una sola evaluación para receptores con efectos laterales.

## #run, reflexión y derive Print

- Evidencia: `cases/73-run/build.log` informa que el sandbox prohíbe
  identificadores y llamadas; `cases/74-fields` usa una lista explícita de
  campos; `cases/75-derive` produce `GridPoint`, sin nombres ni valores.
- Decisión: alcance permitido del evaluador y límites de recursos/efectos;
  superficie de enumeración y visibilidad de campos; formato estable y
  ubicación de la implementación de `derive(Print)`. Son tres decisiones
  distintas y no se implementa una API inferida por semejanza.

## Máscaras SIMD

- Evidencia: `cases/89-mask-convert/build.log` llega a LLVM y rechaza conversión
  de máscara a vector; `89-mask-index` rechaza indexación y `89-mask-select`
  rechaza la máscara como condición escalar.
- `language::simdMaskLane` define verdadero como todos los bits activos y falso
  como cero, con una prueba en `NativeSharp3T13PortableSimdTests.cpp`. No se
  localizó una operación `Select` en `lib/NativeSharp.Core`; el helper interno
  no demuestra que exista una conversión pública ni selección lane a lane.
- Decisión: definir representación de lanes verdaderos y falsos, si existe
  indexación, y una selección lane a lane mediante operador o API explícita.
  Las tres construcciones no son necesariamente la misma capacidad.

## Desvirtualización y AddressSanitizer

- Evidencia: `cases/43-dispatch/optimized.ll` conserva llamada indirecta con
  O2; esto acredita la salida observada, no una obligación de optimización.
- La conclusión anterior sobre ASan era incompleta: existe
  `LlvmNativeSanitizer::instrument`, el pipeline `asan` y la identidad del
  runtime Windows en `src/LLVM/LlvmNativeSanitizer.cpp`. La prueba existente
  `nsc.item389-native-sanitizer` compiló y pasó en esta continuación
  (`asan-existing-build.log`, `asan-existing-test.log`); verifica el IR, no
  una ejecución instrumentada ni un diagnóstico real de fuera de límites.
  No se localizó conexión de esa clase en `src/Driver` ni opción en la ayuda.
- Decisiones: sigue pendiente el objetivo verificable de desvirtualización O2.
  El usuario aprobó exponer ASan como opción explícita con el runtime de Clang
  externo. La ruta para ejecutables de consola se implementó y sus pruebas
  dirigidas pasaron en O0 y O2; restricciones, evidencia y límites de cobertura
  del heap están en `diseno-punteros-y-asan.md`. No se modificó la instalación
  funcional. La DLL y bibliotecas Clang 23 son dependencias de referencia.

## Namespace Text y rutas Compile con ..

- Evidencia: `cases/text-namespace` no encuentra `NativeSharp.Text`;
  `cases/102-outside/build.log` rechaza segmentos `..` en el glob. El SDK sí
  declara `NativeSharp.Core.StringBuilder` y `NativeSharp.Core.Utf8` en
  `lib/NativeSharp.Core`; esto no demuestra que reemplacen toda una API Text.
- Decisiones: el usuario decidió mantener `NativeSharp.Text` pendiente hasta
  definir su superficie y tipos; decidir si un proyecto puede incluir
  archivos fuera de su raíz y bajo qué reglas de resolución y límites.

## Ordenación con comparador

La sobrecarga
`Algorithm.Sort<T>(Span<T>, Comparison<T>)` existe en
`lib/NativeSharp.Core.Collections.Generic/Algorithm.ns`, y
`tests/cc1/c27_algoritmos/Main.ns` comprueba el orden descendente. Una copia
se compiló con el compilador y SDK aislados, con caché desactivada, y salió 42
en O0 y O2 (`sort-existing/result.json`). El ejecutable de pruebas CC1 del
repositorio omite la ejecución real en Windows al no disponer de Wine; por
eso se ejecutó la copia de forma nativa para esta comprobación.
El usuario eligió añadir el alias `SortWith`. El SDK delega en la sobrecarga
con `Comparison<T>`; la reproducción original pasó en O0/O2 y la prueba
`nsc.sdk.sort-with-alias` verificó el orden y casos vacío/unitario en ambos
niveles (`sortwith-original/results.json`, `sortwith-test.log`).

## Preparación de la batería completa

Instrucción posterior del usuario: utilizar siempre compilación incremental
y pruebas dirigidas. Los resultados amplios históricos que siguen no autorizan
repetir una batería completa mientras esa instrucción permanezca vigente.

`ctest --test-dir A:/nsc-fixes-evidencia/build --show-only=json-v1` descubrió
704 entradas; el manifiesto `full-suite-names.txt` tiene SHA-256
`fe1ae8cfed405e5619b5b1427c649ed8ccd50d3eb2748de0f0977222afd12b01`.
El mapa de fuentes y scripts todavía se está auditando para excluir únicamente
la interoperabilidad C/C++ ordenada por el usuario y detectar agregadores que
la invocan indirectamente. `nsc.selfhost.interop-base` y
`nsc.selfhost.interop-parity` aparecen como dependencias en la cadena self-host;
la presencia de `DEPENDS` por sí sola no demuestra qué archivos se consumen.

Una primera clasificación solo conservaba la primera coincidencia por fuente;
se detuvieron las dos construcciones al descubrir el problema y se corrigió el
inventario. Esas dos construcciones no ejecutaron pruebas. Una auditoría
posterior identificó dos pruebas de convenciones de interoperabilidad que sí
se ejecutaron por error en las pasadas unitarias y luego se excluyeron; véase
`campaign-status.md`. El cotejo contra la
clasificación posterior identifica cinco objetos de tests mixtos compilados en
la carpeta de build aislada: `nsc_ns3_t07_error_tests`,
`nsc_ns3_t08_callable_tests`, `nsc_cc1_correctness_tests`,
`nsc_stage46_native_libraries_tests` y
`nsc_stage50_9_metadata_host_compatibility_tests`. No se modificó su código ni
se usó el build de interoperabilidad. Los logs `full-suite-run/build.log` y
`full-suite-run/build2.log` conservan el resultado parcial; la compilación
interrumpida no constituye una batería aprobada.
