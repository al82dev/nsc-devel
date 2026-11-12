# `SortWith` binario: semántica y control de inferencia

La reproducción original del ejemplo 100 retorna 0 sin comprobar los valores
ordenados. `tests/integration/sort_with_binary_semantics.py` complementa aquella
reproducción: extrae mediante AST la cadena `source` existente en
`tests/integration/sort_with_alias.py`, sin editarla, y crea un consumidor que
referencia Core y Collections por sus bibliotecas y metadatos binarios. La fuente
contiene comprobaciones para orden descendente, duplicados, tramo vacío y un
único elemento; retorna 42 solo al superarlas. No se modifica el ejemplo original.

El ejecutor comprueba que los bytes de la variante inferida coincidan con la
cadena extraída y prueba esa fuente con sus dos llamadas inferidas. Por separado
prepara una copia que añade `<int>` exclusivamente a esas dos llamadas. Conserva
la llamada ya explícita que prueba el tramo vacío. Ambos proyectos se crean en
temporales, sin alterar el fixture versionado ni cambiar el producto. Las rutas
de compilador, fixture, bibliotecas, metadata, temporales y modo son argumentos;
reutiliza el análisis de opciones y la ejecución con progreso de
`sort_with_binary_original.py`. El modo `both` exige Clang y runtime ASan; no
admite convertir esa configuración ausente en éxito. CMake no se modifica; el
registro de pruebas compartido corresponde al integrador.

El 12 de noviembre se ejecutó desde `A:/nsc-limitaciones-asan`:
`call ..\nsc-asan-semantic-evidencia\run-sortwith-semantics.cmd`.
El script conserva los argumentos y variables, y su salida completa está en
`A:/nsc-asan-semantic-evidencia/run-sortwith-semantics.log`. Utiliza el `nsc`
de esta copia con SHA-256
`49b8952fb6e0676b5ab35ca09b19df888e64a332ec3fe88cbcc04d81a868a47d`, Core y
Collections binarios de `A:/nsc-asan-sdk-evidencia`, y Clang 23 de referencia.
El log incluye los hashes de todos los argumentos antes y después; coincidieron.
Las compilaciones se ejecutaron sin caché y las salidas y temporales quedaron en A.

| Fuente | Normal O0 | Normal O2 | ASan O0 | ASan O2 |
|---|---|---|---|---|
| Inferida sin cambios | falla en compilación | falla en compilación | falla en compilación | falla en compilación |
| Control con `<int>` | compila 0, ejecuta 42 | compila 0, ejecuta 42 | compila 0, ejecuta 42 | compila 0, ejecuta 42 |

Las cuatro compilaciones inferidas diagnosticaron que no podían inferirse los
argumentos de `SortWith`; no hubo ejecución de esos casos. Por ello el ejecutor
completo devolvió **1**, no éxito. El control demuestra los valores y casos de
la fuente concreta con argumento explícito, no resuelve la inferencia ni cubre
todas las posibles entradas o comparadores. `ASAN_OPTIONS=detect_leaks=0`; Core
y Collections binarios no están instrumentados, por lo que no se demuestra
ausencia de fugas ni instrumentación de sus accesos internos. Los hashes de las
fuentes temporales inferida y explícita aparecen diferenciados en el log.

La siguiente acción es repetir este test y la reproducción original sin
modificar cuando la corrección de genéricos esté disponible e integrada.
El registro y diagnóstico del fallo binario están en
`asan-sortwith-binario-handoff.md`; el `SCOPE LOCK` está en
`A:/nsc-asan-evidencia/SCOPE-LOCK.md`.
