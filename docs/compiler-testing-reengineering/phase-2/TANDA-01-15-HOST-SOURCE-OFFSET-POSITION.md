# Tanda 01.15 — Posición por offset de Host.Source

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Source` |
| Contrato | `CS-SRC-004`, offset de byte a posición |
| Casos | `TS-SRC-019..026` |
| Independencia | No depende de la saturación bloqueada en `TS-SRC-008` |
| Código de producto | Incluir y observar `SourceFile`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

## Contratos ejercitados

- offset cero usa `1:1`;
- los offsets ordinarios incrementan columnas uno-basadas;
- CR y LF pertenecen a la línea anterior hasta consumir el terminador lógico completo;
- el byte posterior al terminador inicia la siguiente línea en columna uno;
- EOF conserva una posición observable y todo offset posterior se limita a ella;
- una secuencia UTF-8 incrementa columnas por bytes, no por code points.

El helper de prueba acepta un `SourceFile` ya construido y una expectativa explícita; no replica `upper_bound`,
no inspecciona `lineStarts_` y no normaliza el buffer. Los fixtures contienen como máximo tres bytes y cada caso
usa la complejidad `O(log L)` del producto sobre una tabla de una o dos líneas.

## Integración

- `SourceOffsetPositionModule.cpp` registra `TS-SRC-019..026` sin declarar tipos auxiliares.
- El manifiesto conserva offsets, bytes y posiciones esperadas.
- El gate dedicado selecciona `CS-SRC-004` con perfil `PR` y exige exactamente ocho resultados ordenados.
- CMake conserva un único ejecutable host y añade un gate de controlador.

## Evidencia de cierre independiente

- Gate dirigido `CS-SRC-004`: 8/8 aprobado en Debug y Release.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 14/14 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 14/14 aprobado.
- La primera pasada de `clang-tidy` detectó tres parámetros `uint32_t` intercambiables en el helper de prueba.
  La expectativa se tipó como `SourcePosition`; la segunda pasada terminó con 0 hallazgos en código propio y
  143 advertencias de dependencias o cabeceras del sistema suprimidas por la herramienta como código ajeno.
- Compilación Python del gate: aprobada.
- Dependencias dinámicas de ambos ejecutables: sólo `KERNEL32.dll`; importaciones de CRT dinámico: 0.
- Diferencial de código de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Los seis archivos preexistentes protegidos conservan exactamente los SHA-256 registrados en el `SCOPE LOCK`.
- Recursión añadida: 0.

La validación fue exclusiva de la suite nueva. No se ejecutaron NSC, NSLink, las 684 pruebas históricas,
minibaselines, baseline, cobertura, instalación ni despliegue. `CS-SRC-004` queda verificado como trabajo
independiente; no elimina ni oculta el bloqueo anterior de `TS-SRC-008` en `CS-SRC-002`.
