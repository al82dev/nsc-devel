# Tanda 01.19 — Consultas de ubicación y rango de Host.Source

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Source` |
| Contrato | `CS-SRC-008`, consultas por ubicación y rango |
| Casos implementados | `TS-SRC-050..058` |
| Código de producto | Incluir y observar `SourceManager`, `SourceFile`, `SourceLocation`, `SourceRange` y `SourcePosition`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

La tanda cierra el contrato independiente `CS-SRC-008`. No altera ni oculta los bloqueos conservados de
`CS-SRC-002`, `CS-SRC-006` y `CS-SRC-007`.

## Contratos ejercitados

- una ubicación cuyo `FileId` no existe produce posición `1:1`, línea vacía y ruta vacía;
- una ubicación válida con offset posterior al buffer se normaliza al EOF antes de calcular posición y línea;
- rangos por defecto, entre archivos distintos o con extremos invertidos producen una vista vacía;
- un rango válido de longitud cero produce una vista vacía sin acceso fuera de límites;
- el rango ordinario `[1,4)` conserva exactamente los bytes `1..3`;
- un extremo final igual al tamaño del buffer se acepta;
- un extremo final mayor que el tamaño del buffer se rechaza sin devolver un prefijo parcial;
- `getLineText` excluye el terminador CRLF al consultar la segunda línea;
- `getPath` devuelve el `displayPath` congelado y no depende del offset ni de mutaciones posteriores del path
  entregado por el llamador.

Cada caso crea su propio `SourceManager`; no comparte buffers, IDs ni vistas con otro caso. Las comprobaciones
usan solamente la interfaz pública real y no duplican la aritmética interna de búsqueda, normalización o slicing.

## Diseño e integración

- `SourceManagerQueryModule.cpp` registra nueve casos cohesionados y un compositor público.
- El módulo separa cada escenario en una función de registro pequeña y no declara tipos auxiliares.
- El manifiesto conserva inputs y oráculos tipados para `TS-SRC-050..058`.
- `run_host_source_manager_query_gate.py` exige exactamente esos nueve IDs, en orden, y rechaza resultados
  faltantes o adicionales.
- CMake agrega el módulo al ejecutable consolidado y un gate dedicado; no crea ejecutables por caso.

Las consultas de posición, línea, rango y path conservan la complejidad del producto. Los fixtures no agregan
recursión, sincronización, acceso privado ni comportamiento exclusivo para pruebas.

## Evidencia de validación

- Gate dirigido `CS-SRC-008`: 9/9 aprobado en Debug y Release.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 18/18 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 18/18 aprobado.
- `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos en código propio; 143 advertencias de
  dependencias o cabeceras del sistema fueron identificadas y suprimidas por la herramienta.
- Compilación en memoria de los 39 archivos Python del controlador: 39/39 aprobada sin generar cachés.
- Dependencias dinámicas de ambos ejecutables: sólo `KERNEL32.dll`; importaciones de CRT dinámico: 0.
- Diferencial de código de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Los seis archivos preexistentes protegidos conservan exactamente sus SHA-256 registrados.
- Tipos nuevos declarados: 0.
- Recursión añadida: 0.

La validación fue exclusiva de la suite nueva. No se ejecutaron NSC, NSLink, las 684 pruebas históricas,
minibaselines, baseline, cobertura, sanitizadores, instalación ni despliegue. El resultado verificable es el
cierre completo de los nueve casos diseñados para `CS-SRC-008`.

## Trazabilidad

| Caso | Símbolo observado | Oráculo |
|---|---|---|
| `TS-SRC-050` | `getPosition`, `getLineText`, `getPath` | sentinelas para archivo inválido |
| `TS-SRC-051` | `getPosition`, `getLineText` | posición `2:2` y línea `b` en EOF |
| `TS-SRC-052` | `getText` | vista vacía para tres clases de rango inválido |
| `TS-SRC-053` | `SourceRange::isValid`, `getText` | rango válido vacío |
| `TS-SRC-054` | `getText` | slice exacto `bcd` |
| `TS-SRC-055` | `getText` | slice exacto `cde` hasta tamaño |
| `TS-SRC-056` | `getText` | rechazo íntegro al exceder tamaño |
| `TS-SRC-057` | `getLineText` | segunda línea `second` sin CRLF |
| `TS-SRC-058` | `getPath` | path congelado y offset ignorado |

Diagramas: no aplica. La tanda agrega casos a la arquitectura aprobada sin introducir un flujo o dependencia de
producto nuevos.
