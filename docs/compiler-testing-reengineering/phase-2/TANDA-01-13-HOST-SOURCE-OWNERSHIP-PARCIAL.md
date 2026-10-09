# Tanda 01.13 — Ownership acotado de SourceFile

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Source` |
| Contrato | `CS-SRC-002`, snapshot inmutable y propietario de `SourceFile` |
| Casos implementados | `TS-SRC-003..007` |
| Caso bloqueado | `TS-SRC-008` |
| Código de producto | Incluir y observar `FileId` y `SourceFile`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

## Contratos ejercitados

- `TS-SRC-003` conserva exactamente ID, ruta, `displayPath` y buffer ASCII.
- `TS-SRC-004` conserva una ruta Unicode representable y fija que `displayPath == path.string()`.
- `TS-SRC-005` demuestra que NUL y `0xff` son bytes ordinarios tanto en la vista como en `size()`.
- `TS-SRC-006` destruye las entradas movidas antes de consultar el archivo y demuestra ownership independiente.
- `TS-SRC-007` obtiene las vistas dos veces sin mutación y exige contenido y punteros estables.

Todos los casos son deterministas, no escriben archivos, no declaran tipos auxiliares y se ejecutan dentro del
único binario modular `nsc_component_tests`. Su costo es lineal respecto de fixtures inferiores a 128 bytes,
excepto por la conversión de ruta que pertenece al constructor observado.

## BLOQUEADO

Motivo: `TS-SRC-008` sólo puede alcanzar la rama de saturación si `SourceFile` posee un `std::string` cuyo tamaño
sea mayor que `UINT32_MAX`. El constructor llama inmediatamente a `buildLineStarts()`, que recorre todos los
bytes. Una prueba real exige por tanto reservar, inicializar y recorrer más de 4 GiB; no existe un seam público,
una representación virtual ni una dependencia inyectable que permita comprobar la rama con memoria acotada.

Fuente: `SourceFile::SourceFile`, `SourceFile::size` y `SourceFile::buildLineStarts` en
`src/Source/SourceFile.cpp`; diseño `TS-SRC-008` en `HOST-SOURCE-TEST-PLAN.md`.

Impacto: `CS-SRC-002` permanece abierto. Esta tanda implementa cinco de sus seis casos y no autoriza avanzar el
cursor al contrato siguiente ni declarar cerrado `Host.Source`.

Información faltante: una decisión funcional posterior que autorice una frontera de tamaño verificable sin
cambiar el contrato observable, o autorización expresa para una prueba WEEKLY que consuma más de 4 GiB y asuma
el costo completo de construcción e indexado.

Acción necesaria: conservar `TS-SRC-008` como bloqueo visible; no simular estado privado, no usar comportamiento
indefinido, no duplicar la condición en un helper de prueba y no crear un fixture proporcional al espacio libre.

## Integración

- `SourceFileOwnershipModule.cpp` registra sólo `TS-SRC-003..007`.
- El manifiesto declara únicamente esos cinco casos ejecutables; no representa `TS-SRC-008` como implementado.
- El gate dedicado selecciona `CS-SRC-002` con perfil `NIGHTLY` y exige exactamente los cinco casos disponibles.
- CMake incorpora `SourceFile.cpp` al mismo binario host y añade un único gate de controlador.

## Evidencia de la tanda

- Gate dirigido `CS-SRC-002` disponible: 5/5 aprobado en Debug y Release.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 12/12 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 12/12 aprobado.
- Análisis `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos en código propio; 143 advertencias de
  dependencias o cabeceras del sistema fueron identificadas y suprimidas por la herramienta como código ajeno.
- Compilación Python del gate: aprobada.
- Dependencias dinámicas de ambos ejecutables: sólo `KERNEL32.dll`; importaciones de CRT dinámico: 0.
- Diferencial de código de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Los seis archivos preexistentes protegidos conservan exactamente los SHA-256 registrados en el `SCOPE LOCK`.
- Líneas mayores a 120 caracteres en los tres archivos nuevos editables: 0.
- Recursión añadida: 0.

La validación fue exclusiva de la suite nueva. No se ejecutaron el compilador NativeSharp, las 684 pruebas
históricas, minibaselines, baseline, cobertura, instalación ni despliegue. La evidencia no cierra `CS-SRC-002`
porque `TS-SRC-008` continúa bloqueado por la frontera de memoria descrita arriba.
