# Tanda 01.14 — Índice iterativo de líneas de Host.Source

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Source` |
| Contrato | `CS-SRC-003`, índice iterativo de líneas |
| Casos | `TS-SRC-009..018` |
| Independencia | No depende de la saturación bloqueada en `TS-SRC-008` |
| Código de producto | Incluir y observar `SourceFile`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

## Oráculo observable

`lineStarts_` es privado. La prueba no viola encapsulamiento ni duplica la implementación: exige primero el
`lineCount()` exacto y después consulta `positionForOffset()` en cada comienzo esperado. Cada comienzo debe
resolver a su línea uno-basada y columna uno; el byte inmediatamente anterior debe conservarse en la línea
previa. Este par de observables detecta comienzos omitidos, añadidos o desplazados.

La matriz cubre buffer vacío, texto sin terminadores, LF, CR, CRLF, terminadores finales, mezcla consecutiva y
bytes NUL/`0xff`. Todos los fixtures son secuencias explícitas de bytes, se recorren iterativamente y consumen
memoria `O(n + L)` propia del producto para `n <= 4` y `L <= 4`.

## Contradicción resuelta sin reescribir la fuente

El diseño de Fase 1 asignó cinco líneas a `TS-SRC-017` para `\n\r\r\n`. La implementación, el documento de
componente y el contrato coinciden en que `CRLF` es un único salto. El recorrido real es:

1. `LF` en 0 registra comienzo 1;
2. `CR` en 1 registra comienzo 2;
3. `CRLF` en 2..3 registra comienzo 4.

Por tanto existen tres terminadores lógicos y cuatro líneas con comienzos `[0,1,2,4]`. La discrepancia queda
registrada como `CON-0018`; el test plan original se conserva intacto como evidencia.

## Integración

- `SourceLineIndexModule.cpp` registra la matriz completa `TS-SRC-009..018`.
- El manifiesto conserva los diez IDs, buffers explícitos y comienzos esperados.
- El gate dedicado selecciona `CS-SRC-003` con perfil `PR` y exige exactamente diez resultados ordenados.
- CMake conserva un solo ejecutable host y añade un gate de controlador.

## Evidencia de cierre independiente

- Gate dirigido `CS-SRC-003`: 10/10 aprobado en Debug y Release.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 13/13 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 13/13 aprobado.
- Análisis `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos en código propio; 143 advertencias de
  dependencias o cabeceras del sistema fueron identificadas y suprimidas por la herramienta como código ajeno.
- Compilación Python del gate: aprobada.
- Dependencias dinámicas de ambos ejecutables: sólo `KERNEL32.dll`; importaciones de CRT dinámico: 0.
- Diferencial de código de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Los seis archivos preexistentes protegidos conservan exactamente los SHA-256 registrados en el `SCOPE LOCK`.
- Líneas mayores a 120 caracteres en los tres archivos nuevos editables: 0.
- Recursión añadida: 0.

La validación fue exclusiva de la suite nueva. No se ejecutaron NSC, NSLink, las 684 pruebas históricas,
minibaselines, baseline, cobertura, instalación ni despliegue. `CS-SRC-003` queda verificado como trabajo
independiente; no elimina ni oculta el bloqueo anterior de `TS-SRC-008` en `CS-SRC-002`.
