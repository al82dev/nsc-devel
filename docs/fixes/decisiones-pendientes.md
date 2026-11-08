# Decisiones de diseño pendientes

Revisión observada: `ab5870aa8a97008602c2d4d8631ea3c277d7284b` en
`A:/nsc-devel-fixes`. Las reproducciones originales y sus resultados se
conservan en `cases`; ninguna propuesta de este archivo está implementada por
el hecho de aparecer aquí. Las preguntas ya enviadas están pendientes de
respuesta mientras se continúa la validación independiente.

## checked sobre i128

- Evidencia: `cases/comp-03/result.json` muestra que la suma de uno al máximo
  termina normalmente con wrapping; el evaluador de políticas rechaza el
  overflow checked en `src/Language/NumericSemantics.cpp`.
- Decisión: expresar el fallo en runtime mediante una trampa de overflow o
  mediante una excepción. Recomendación inicial: trampa, si el contrato del
  lenguaje no exige recuperación por excepción. La reproducción demuestra el
  wrapping, no cuál de los mecanismos es obligatorio.
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
  `Drop()` para abandono. Decisión: confirmar ese contrato o exigir limpieza
  inmediata al llamar a `Cancel()`; son comportamientos observables distintos.

## Punteros compartidos

- Evidencia: `cases/sdk-01` no resuelve Rc, WeakRc, Arc ni WeakArc. El
  manifiesto de `src/Language/SmartPointerSemantics.cpp` describe familias,
  operaciones, destrucción, módulo y concurrencia; `lib/NativeSharp.Core.Threading/ReferenceCounting.ns`
  implementa únicamente estado de contadores y hooks, no esas familias.
- Decisión: confirmar implementación pública de las cuatro familias y su
  ubicación, inicialización, representación, errores, ownership y duración.
  No se debe confundir un manifiesto o contadores con una implementación.

## impl para struct y métodos genéricos en tipos genéricos

- Evidencia: `cases/68-struct/build.log` exige como destino una clase no
  estática local. `cases/70-method/build.log` rechaza expresamente métodos
  genéricos dentro de tipos genéricos; la función externa corregida en la
  campaña anterior no implementa ese contrato.
- Decisión: confirmar si `impl` externo para estructuras debe admitirse con
  despacho estático y sin boxing; confirmar por separado si se habilita la
  especialización combinada del tipo y del método, incluidos límites y errores.

## Restricciones entre parámetros genéricos

- Evidencia: `cases/69-constraint` reproduce `unknown type name 'T'` dentro
  de `Reducer<T>`. El comentario en
  `src/Frontend/NameLookup/SyntaxBinding.cpp` supone que otra validación no
  permite referencias a otros parámetros, aunque esta forma anidada llega al
  resolvedor. La muestra del reporte fue reconstruida.
- Decisión: especificar si un constraint de un parámetro puede depender de
  otro y cómo se comprueba después de especializar. No basta omitir el error de
  nombres: debe existir una comprobación de satisfacción del tipo construido.

## Temporales en parámetros in y operadores ++/--

- Evidencia: `cases/70-temporary` rechaza el temporal; `cases/39-increment`
  explica que la sobrecarga podría evaluar dos veces el operando al almacenar
  el resultado. El caso decremento también es rechazado.
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
- Decisiones: objetivo verificable de desvirtualización O2; si ASan debe
  exponerse desde el CLI, sintaxis, disponibilidad y forma de encontrar y
  enlazar el runtime sin modificar la instalación funcional.

## Namespace Text y rutas Compile con ..

- Evidencia: `cases/text-namespace` no encuentra `NativeSharp.Text`;
  `cases/102-outside/build.log` rechaza segmentos `..` en el glob. El SDK sí
  declara `NativeSharp.Core.StringBuilder` y `NativeSharp.Core.Utf8` en
  `lib/NativeSharp.Core`; esto no demuestra que reemplacen toda una API Text.
- Decisiones: confirmar si `NativeSharp.Text` es una superficie pública
  requerida y qué tipos contiene; decidir si un proyecto puede incluir
  archivos fuera de su raíz y bajo qué reglas de resolución y límites.

## Ordenación con comparador

No requiere crear `SortWith` para demostrar la operación: la sobrecarga
`Algorithm.Sort<T>(Span<T>, Comparison<T>)` existe en
`lib/NativeSharp.Core.Collections.Generic/Algorithm.ns`, y
`tests/cc1/c27_algoritmos/Main.ns` comprueba el orden descendente. Una copia
se compiló con el compilador y SDK aislados, con caché desactivada, y salió 42
en O0 y O2 (`sort-existing/result.json`). El ejecutable de pruebas CC1 del
repositorio omite la ejecución real en Windows al no disponer de Wine; por
eso se ejecutó la copia de forma nativa para esta comprobación.

## Preparación de la batería completa

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
inventario. No se ejecutaron pruebas de interoperabilidad. El cotejo contra la
clasificación posterior identifica cinco objetos de tests mixtos compilados en
la carpeta de build aislada: `nsc_ns3_t07_error_tests`,
`nsc_ns3_t08_callable_tests`, `nsc_cc1_correctness_tests`,
`nsc_stage46_native_libraries_tests` y
`nsc_stage50_9_metadata_host_compatibility_tests`. No se modificó su código ni
se usó el build de interoperabilidad. Los logs `full-suite-run/build.log` y
`full-suite-run/build2.log` conservan el resultado parcial; la compilación
interrumpida no constituye una batería aprobada.
