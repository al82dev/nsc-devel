# Tanda 19: línea base de las pruebas de NSC (T19-027, T19-037)

## Método

1. Se construyó NSC en `152654e5a` (2026-10-27), el último commit común con `development`, anterior a la
   interoperabilidad. Se hizo en un worktree propio (`build-claude-t08/base-152654e5a`), con las mismas
   opciones que `build-tanda09`.
2. En ese árbol se aplicó, sin commit, la dependencia `nsc_frontend → nsc_precompiled` del
   [arreglo de enlace](t19-enlace-pruebas.md): el defecto ya existía en ese commit.
3. Las pruebas del árbol base usan su propio `nsc.exe` (`NSC_TEST_COMPILER`).
4. Se ejecutó el CTest completo de cada árbol con 8 procesos y se compararon con `comparar-ctest.py`:
   - base: `logs/ctest-completo-base.log`;
   - ahora: `logs/ctest-completo-lb1.log`.

## Resultado

| | Base `152654e5a` | Ahora (`0acb10330`) |
|---|---|---|
| Pruebas | 694 | 704 |
| Pasan | 576 | 592 |
| Fallan | 118 (4 con `SEGFAULT`) | 112 |

Clasificación de las 112 que fallan ahora:

| Grupo | Pruebas | Qué significa |
|---|---|---|
| Fallan también en la base | 107 | Defectos anteriores a la interoperabilidad, con la misma prueba fallando en `152654e5a`. Ahí están las 6 que antes no se enlazaban y las unitarias de T19-027 que no son una de las 3 regresiones. |
| Pasan en la base y fallan ahora | 3 | Regresiones del rango de la interoperabilidad (ver abajo). |
| Pruebas nuevas de la interoperabilidad que fallan | 2 | `nsc.cpp-interop.session` dice `C/C++ session fixtures are missing beneath A:/temp/t09-session-root`; `nsc.cpp-interop.record-value-observation` falla sin imprimir el motivo. Por investigar. |

Otras 11 pruebas fallaban en la base y pasan ahora. Ninguna prueba de la base desapareció.

## Regresiones del rango de la interoperabilidad

| Prueba | Comprobación que falla | Causa en el código |
|---|---|---|
| `nsc.windows-runtime` | `runtime defines the MSVC floating-point marker symbol` | `c1cbdc5a2` (2026-11-06) emite `_fltused` con enlace `weak_odr`, para evitar el duplicado con `libcmt.lib` al enlazar con LLD. La prueba fija `@_fltused = global i32 0`. |
| `nsc.precompiled-semantic-snapshot-codec` | `another schema is rejected` | `fc065e44a` y `644b0af5e` (2026-11-05) cambiaron el esquema de `-v1` a `-v3` al agregar campos `volatile` en los codecs de parámetros y miembros. La prueba altera `-v1`, que ya no existe. |
| `nsc.stage52-1.generic-collections-additions` | `project_stage52 ... lowers to OOP MIR` | Por investigar. `stage52-6` (que ya fallaba en la base) muestra en `SortedList.ns` `indirect call argument would discard pointer pointee const qualification` para `comparer(in a, in b)`. |

Estas tres se corrigen en el compilador, sin tocar las pruebas.

## Las 107 anteriores

Fallan igual en `152654e5a`, así que no las causó la interoperabilidad. Siguen siendo defectos de NSC: T19-037
las conserva visibles. No se marcan como aceptadas.

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
