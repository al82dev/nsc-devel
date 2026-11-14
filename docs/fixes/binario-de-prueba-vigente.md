# Binarios de prueba vigentes (NSC-NEW-002)

`ctest` no construye. Muchas suites enlazan las bibliotecas del compilador en su propio
ejecutable y ejecutan el `Driver` dentro del proceso; las pruebas Python ejecutan `nsc.exe`.
Después de construir solo `--target nsc`, esos ejecutables conservan el compilador anterior y la
corrida informa su veredicto como si fuera el actual, en las dos direcciones: defectos ya
corregidos aparecen como fallos y defectos nuevos pueden pasar.

Caso medido el 2026-11-14: `tests/nsc_cc1_correctness_tests.exe` era del 2026-11-08, anterior a
`648b20d46` (traits sobre structs genéricos) y `0f25b9021` (`Arc`/`WeakArc` con `Send`/`Sync`).
52 de 71 fixtures fallaban con "sealed trait coherence rejects this generic impl", mientras el
`nsc.exe` vigente compilaba el mismo proyecto con salida 0. Al reconstruir el ejecutable quedaron
7 fallos, todos reales y descritos en sus propios documentos. `nsc_ns3_t09_iteration_borrow_tests`
estaba en la misma situación.

## Cambio

- `cmake/VerifyTestBinaryCurrent.cmake` falla cuando una entrada del enlace (biblioteca estática del
  proyecto, directa o transitiva) o una fuente propia del ejecutable es más reciente que el
  ejecutable, o cuando el ejecutable o una entrada no existen. El mensaje nombra las entradas y el
  objetivo que hay que construir.
- Cada ejecutable del directorio, incluido `nsc`, recibe la prueba `nsc.binary-current.<objetivo>`
  con `FIXTURES_SETUP`. Cada prueba que lo ejecuta, como `COMMAND` o mediante `$<TARGET_FILE:...>`,
  lo requiere con `FIXTURES_REQUIRED`. `ctest -R` incorpora el guardia aunque el patrón no lo
  nombre, y un binario obsoleto deja sus pruebas en "Not Run" con la razón.
- El mapa prueba-objetivo se lee del propio `CMakeLists.txt`, como la lista de fixtures cc1 se lee
  de su `.cpp`. Si la lectura reconoce menos de 200 pruebas, el configure se detiene. Las pruebas
  cc1, cuyo nombre se arma en un bucle, declaran el requisito de forma explícita.
- `nsc_basic` contiene `src/Basic/Version.cpp`, que `nsc_build_identity` regenera en cada build:
  la biblioteca se reenlaza en cualquier build y su fecha no dice nada del compilador. La
  representan sus otras fuentes. Sin esta excepción, construir un objetivo dejaba obsoletos a todos
  los demás.
- Los encabezados no son entradas: un cambio de encabezado reconstruye las bibliotecas que lo
  incluyen. Las pruebas que usan `NSC_TEST_COMPILER` no se asocian, porque puede ser un compilador
  externo.

## Verificación

- Con el ejecutable cc1 recién enlazado, `nsc.cc1.correctness.binary-current` pasó.
- Tras `cmake -E touch lib/nsc_frontend.lib`, el guardia falló y nombró esa biblioteca; las
  fixtures que lo requieren quedaron "Not Run". Al reconstruir el objetivo, volvió a pasar.
- El configure registró 452 guardias y 428 pruebas asociadas. En el build aislado, 399 ejecutables
  eran anteriores a sus bibliotecas y 47 no estaban construidos: los resultados previos de esas
  suites no describen el compilador actual.
- `nsc_ns3_t09_iteration_borrow_tests`, reconstruido, mostró un fallo real que el binario viejo
  también tenía: su preludio de `Span` no declaraba `RefAt`, que `foreach (ref ...)` usa desde el
  congelamiento 3.0 P4 (`80b186758`). El preludio lo declara ahora; el caso de lectura pasó a
  aceptarse y el de mutación sigue rechazado con "could invalidate yielded references".
