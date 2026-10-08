# Tanda 14 — Metadata, proyectos y driver

## Alcance

La tanda clasificó los 123 IDs de `metadata`, `project` y `driver`. No modificó código de
producción, pruebas históricas, `nscbuilder.py` ni herramientas instaladas. Sólo se ejecutó O0 y no
se ejecutaron baseline, certificación, promoción o instalación.

## Inventario cerrado

| Clase | IDs |
|---|---:|
| Universo metadata/project/driver | 123 |
| C++ O0 consolidado y ejecutado | 75 |
| Fixtures project O0 externalizadas | 5 |
| C++ con rutas O1/O2/O3 | 34 |
| Proyecto directo O2 | 1 |
| Operaciones prohibidas por Scope Lock | 7 |
| Verificador especializado enrutado a tanda 16 | 1 |
| Total | 123 |

Los 35 contratos no-O0 permanecen intactos y trazables. Los siete contratos de instalación,
baseline, certificación o promoción no se ejecutaron. `nsc.ns3.t44.metadata-closure` se conserva
para el `SuiteAdapter` de la tanda 16.

## Implementación

- `ProjectDriverMetadataUnitTests.cpp` registra 75 propietarios históricos.
- `ProjectDriverMetadataLegacyEntries.def` fija la membresía y orden de ejecución.
- `project_driver_metadata_adapters/*.cpp` preserva las aserciones originales cambiando sólo el
  símbolo `main`.
- `tests/cases/project` externaliza Stage25 a Stage29 con topología autocontenida. Stage25 conserva
  exactamente sus subproyectos `App` y `Core`; no se aplanó su `ProjectReference`.

## Precondiciones verificadas

La primera corrida de `driver` encontró 37 fallos con una sola causa: los propietarios heredados
intentaron abrir el runtime por defecto de una instalación C:. La repetición fijó explícitamente:

- `NSC_LINKER=A:\nslink-devel\bin\release\nslink.exe`;
- `NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-devel\build-clang\generated\NativeSharp.Runtime.Win32.lib`;
- `NSC_LLVM_LIB` al `llvm-lib.exe` ya disponible.

Con esas precondiciones, `driver` pasó 53/53. No se copió ni modificó nada en C:.

`nsc.ns3.t33.package-coherence` expuso además que el PATH heredado, extraordinariamente largo,
impedía que `std::system` resolviera `cmd.exe`. El caso aislado y luego el módulo completo pasaron
con un PATH mínimo que contiene `C:\Windows\System32` y LLVM. La lógica del caso no cambió.

## Validación dirigida

| Validación | Resultado |
|---|---:|
| Compilación de `nsc_unit_tests` | correcta |
| Metadata C++ O0 | 8/8 |
| Project C++ O0 | 14/14 |
| Driver C++ O0 | 53/53 |
| Fixtures project O0 | 5/5 |
| Total ejecutado en la tanda | 80/80 |

La primera materialización de Stage25 falló porque se había copiado sólo `App`; el reporte indicó
la ausencia de `../Core/Core.nsproj`. La fixture se corrigió conservando ambos subproyectos y la
repetición completa pasó 5/5.

## Complejidad y propiedad

El registro es iterativo O(n), con O(1) memoria auxiliar. Cada propietario histórico mantiene su
propiedad y limpieza original. Las cinco fixtures externas usan workspaces independientes bajo A:
y el limpiador iterativo del harness.

## Archivos creados

- Un módulo y un inventario `.def` para la familia.
- 75 adaptadores de transición, uno por propietario C++.
- Cinco manifiestos project y sus fuentes/proyectos autocontenidos.

No se creó otro runner por componente; todo se integra en los dos ejecutores compartidos ya
definidos por la arquitectura nueva.
