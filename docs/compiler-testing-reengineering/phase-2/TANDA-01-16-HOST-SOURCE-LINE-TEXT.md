# Tanda 01.16 — Extracción de línea lógica de Host.Source

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Source` |
| Contrato | `CS-SRC-005`, extracción de línea lógica |
| Casos | `TS-SRC-027..033` |
| Independencia | No depende de la saturación bloqueada en `TS-SRC-008` |
| Código de producto | Incluir y observar `SourceFile`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

## Contratos ejercitados

- la línea cero y una línea posterior al conteo producen vista vacía;
- una línea ordinaria conserva exactamente sus bytes;
- LF, CR y CRLF no aparecen en el texto devuelto;
- una línea vacía intermedia permanece contada aunque su vista sea vacía;
- un terminador final crea una última línea vacía válida;
- límites CRLF consecutivos conservan la línea vacía entre ellos y retiran ambos bytes de cada terminador.

El helper sólo compara `string_view` con bytes esperados. No copia el algoritmo de indexación ni accede a
`lineStarts_`. Los fixtures contienen como máximo 16 bytes; cada consulta conserva la complejidad y lifetime
del producto y se evalúa mientras el `SourceFile` propietario permanece vivo.

## Integración

- `SourceLineTextModule.cpp` registra `TS-SRC-027..033` sin declarar tipos auxiliares.
- El manifiesto conserva línea consultada, buffer y slice esperado.
- El gate dedicado selecciona `CS-SRC-005` con perfil `PR` y exige exactamente siete resultados ordenados.
- CMake conserva un único ejecutable host y añade un gate de controlador.

## Evidencia de cierre independiente

- Gate dirigido `CS-SRC-005`: 7/7 aprobado en Debug y Release.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 15/15 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 15/15 aprobado.
- Análisis `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos en código propio; 143 advertencias de
  dependencias o cabeceras del sistema fueron identificadas y suprimidas por la herramienta como código ajeno.
- Compilación Python del gate: aprobada.
- Dependencias dinámicas de ambos ejecutables: sólo `KERNEL32.dll`; importaciones de CRT dinámico: 0.
- Diferencial de código de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Los seis archivos preexistentes protegidos conservan exactamente los SHA-256 registrados en el `SCOPE LOCK`.
- Líneas mayores a 120 caracteres en los tres archivos nuevos editables: 0.
- Recursión añadida: 0.

La validación fue exclusiva de la suite nueva. No se ejecutaron NSC, NSLink, las 684 pruebas históricas,
minibaselines, baseline, cobertura, instalación ni despliegue. `CS-SRC-005` queda verificado como trabajo
independiente; no elimina ni oculta el bloqueo anterior de `TS-SRC-008` en `CS-SRC-002`.
