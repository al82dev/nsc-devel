# Tanda 01.12 — Valor SourcePosition de Host.Source

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Source` |
| Contrato | `CS-SRC-001`, valor `SourcePosition` |
| Casos | `TS-SRC-001` y `TS-SRC-002` |
| Código de producto | Sólo incluir y observar `SourcePosition.h`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

## Contratos ejercitados

`TS-SRC-001` construye `SourcePosition{}` y exige línea 1 y columna 1. `TS-SRC-002` usa inicialización agregada
con `{7,11}` y comprueba que ambos valores se conservan sin normalización. Los casos fijan exclusivamente la
base uno y la semántica de valor descritas por `CS-SRC-001`.

## Integración

- `SourcePositionModule.cpp` registra los dos casos sin declarar tipos auxiliares.
- El manifiesto conserva los dos IDs, su contrato, entradas y oráculos escalares.
- El gate dedicado selecciona `CS-SRC-001` con perfil `PR` y exige ambos resultados en orden.
- CMake conserva un único ejecutable host y añade un gate de controlador.

## Evidencia de cierre

- Gate dirigido `CS-SRC-001`: 2/2 aprobado en Debug y Release.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 11/11 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 11/11 aprobado.
- Análisis `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos en código propio; 143 advertencias de
  dependencias o cabeceras del sistema fueron identificadas y suprimidas por la herramienta como código ajeno.
- Diferencial de código de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Recursión añadida: 0.

La validación fue exclusiva de la suite nueva. No se ejecutaron el compilador NativeSharp, las 684 pruebas
históricas, minibaselines, baseline, cobertura, instalación ni despliegue.
