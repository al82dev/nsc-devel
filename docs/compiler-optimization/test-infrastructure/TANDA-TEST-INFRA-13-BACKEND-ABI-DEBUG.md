# Tanda 13 — LLVM, target, ABI, assembly y debug-info

## Alcance cerrado

Esta tanda migró la familia backend sin tocar el compilador de producción, sin modificar la
infraestructura histórica y sin ejecutar baseline, certificación, promoción, instalación ni
optimización distinta de O0. El universo canónico fue la matriz de 684 IDs, filtrada por los
componentes `assembly`, `debug-info`, `llvm` y `target`.

## Inventario exacto

| Clase | IDs |
|---|---:|
| Universo backend de la matriz | 93 |
| C++ O0 incorporado al ejecutor consolidado | 29 |
| Fixtures externas O0 incorporadas al catálogo | 20 |
| C++ con rutas O1/O2/O3, bloqueado por alcance | 29 |
| Directas O1/O2/O3, bloqueadas por alcance | 5 |
| Verificadores CMake multietapa enrutados a tanda 16 | 8 |
| Casos con precondición histórica externa reproducida | 2 |
| Total clasificado | 93 |

No se cambió ninguna expectativa O1/O2/O3 para hacerla pasar en O0. Esos 34 IDs conservan su
contrato histórico y quedan explícitamente `BLOCKED_BY_SCOPE_O0_ONLY`.

## Implementación

- `BackendUnitTests.cpp` registra iterativamente los contratos C++ O0.
- `BackendLegacyEntries.def` es la membresía determinista de 29 IDs.
- `backend_adapters/*.cpp` cambia únicamente el símbolo de entrada de cada propietario histórico;
  las aserciones originales siguen siendo la autoridad.
- `tests/cases/backend` contiene 20 fixtures autocontenidas: seis fuentes, doce proyectos y dos
  consultas puras de target/codegen.
- El runner siempre recibió el NSC construido en `A:\nsc-devel\build-clang\bin\nsc.exe` y NSLink
  explícito en `A:\nslink-devel\bin\release\nslink.exe`.

## Precondiciones históricas no ocultadas

`nsc.item308.volatile-mmio` y `nsc.stage52-6.foreach-collections-enumerators` se ejecutaron primero
mediante el adaptador. El primero se volvió a construir y ejecutar con su binario CTest histórico;
reprodujo las mismas tres fallas porque no produjo `VolatileProducer.lib`. El segundo conserva una
referencia a artefactos `NativeSharp.Core.Collections.Generic` que no existen en el árbol de fuente.
Ninguno se contó como aprobado ni se parcheó con artefactos obsoletos. Ambos quedan diagnosticados
como bloqueo de precondición existente.

## Validación dirigida

| Validación | Resultado |
|---|---:|
| Compilación de `nsc_unit_tests` | correcta |
| C++ assembly O0 | 1/1 |
| C++ debug-info O0 | 4/4 |
| C++ target/ABI O0 | 9/9 |
| C++ LLVM O0 incorporado | 15/15 |
| Fixtures LLVM O0 | 19/19 |
| Fixture target O0-compatible | 1/1 |
| Catálogo backend | 20/20 manifiestos válidos |

La primera ejecución target falló en tres contratos por no haber exportado `NSC_LINKER`. La
repetición con la ruta explícita de A: pasó 9/9; esto confirma una precondición del ejecutor y no un
cambio de lenguaje. No se ejecutó ninguna configuración O1, O2 u O3.

## Archivos nuevos y motivo

- `tests/unit/modules/BackendUnitTests.cpp`: módulo consolidado de contratos internos backend.
- `tests/unit/include/nsc_test/BackendLegacyEntries.def`: inventario auditable de entradas.
- `tests/unit/backend_adapters/*.cpp`: transición sin duplicar lógica histórica.
- `tests/cases/backend/**`: fuentes, proyectos y manifiestos externos autocontenidos.

## Complejidad y propiedad

El registro recorre 29 entradas en O(n) y O(1) memoria auxiliar. Cada ejecución sigue siendo
sincrónica y conserva la propiedad de recursos de su prueba original. El catálogo materializa cada
fixture en un workspace aislado de A: y lo limpia iterativamente.

## Pendiente enrutado

Los ocho verificadores especializados se integrarán mediante `SuiteAdapter` en la tanda 16. Los 34
contratos no-O0 quedan fuera de ejecución mientras rija el alcance O0. Los dos bloqueos por
artefactos externos deberán resolverse en la tanda propietaria de runtime/librerías, no mediante
fallbacks ni copias desde C:.
