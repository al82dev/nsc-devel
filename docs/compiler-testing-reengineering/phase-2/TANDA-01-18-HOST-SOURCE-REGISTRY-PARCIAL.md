# Tanda 01.18 — Registro e identidad de Host.Source

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Source` |
| Contrato | `CS-SRC-007`, registro, identidad y lookup |
| Casos implementados | `TS-SRC-045..048` |
| Caso bloqueado | `TS-SRC-049` |
| Código de producto | Incluir y observar `SourceManager` y `SourceFile`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

La tanda es independiente de los bloqueos anteriores. No cierra `CS-SRC-007`: implementa cuatro de sus cinco
casos y conserva el caso de sanitizador como bloqueo explícito.

## Contratos ejercitados

- el primer y segundo `addBuffer` asignan `FileId{1}` y `FileId{2}`;
- cada ID recupera el snapshot que le corresponde;
- dos rutas con los mismos bytes visibles reciben identidades y objetos distintos;
- el ID cero y el primer ID mayor que el conteo producen `nullptr`;
- un ID válido recupera un objeto con identidad, path y bytes exactos.

Cada caso crea su propio `SourceManager`, por lo que no comparte estado, memoria ni identidad con otro caso. Los
buffers son pequeños y la construcción conserva el algoritmo real de `SourceFile`, incluido su índice de líneas.
No se inspecciona `files_`, no se replica la aritmética de índices y no se agrega acceso exclusivo para pruebas.

## Bloqueo conservado

### `TS-SRC-049` — lifetime después de realocación

El diseño de Fase 1 define este caso como `sanitizer-only misuse probe`: conservar un puntero devuelto por
`getFile`, provocar una realocación del vector y comprobar que una desreferencia posterior es detectada por el
sanitizador. Los builds autorizados de esta suite son Debug y Release con runtime estático; no existe un perfil
ASan versionado ni un protocolo de subprocess que convierta el fallo intencional en un resultado causal.

Comparar el valor de un puntero invalidado no demuestra que su uso sea seguro o inseguro. Desreferenciarlo dentro
del host normal introduce comportamiento indefinido y podría abortar toda la suite en vez de producir un oráculo
estructurado. Añadir un ejecutable especial, cambiar flags o ampliar el controlador sería infraestructura nueva no
definida para esta tanda.

Estado: `BLOQUEADO_POR: perfil de sanitizador y aislamiento de misuse probe no implementados`.

No se añadió `SKIP`, comparación engañosa de direcciones, desreferencia inválida ni cambio en `SourceManager`.

## Diseño e integración

- `SourceManagerRegistryModule.cpp` contiene cuatro funciones de registro cohesionadas y un compositor público.
- El manifiesto conserva inputs, oráculos y tags únicamente para los cuatro casos ejecutables.
- `run_host_source_manager_registry_gate.py` exige exactamente `TS-SRC-045..048` en orden.
- CMake integra el módulo y un gate dedicado sin crear un ejecutable por prueba.
- El módulo reutiliza los tipos públicos reales y no declara tipos auxiliares.

Cada función permanece debajo de 60 líneas ejecutables. Las operaciones son `O(1)` para lookup y amortizadamente
`O(1)` más la indexación lineal del buffer para registro; los fixtures no intentan alterar esa complejidad.

## Evidencia de validación

- Gate dirigido `CS-SRC-007`: 4/4 aprobado en Debug y Release.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 17/17 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 17/17 aprobado.
- `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos en código propio; 143 advertencias de
  dependencias o cabeceras del sistema fueron identificadas y suprimidas por la herramienta.
- Compilación en memoria del gate Python: aprobada sin generar cachés.
- Dependencias dinámicas de ambos ejecutables: sólo `KERNEL32.dll`; importaciones de CRT dinámico: 0.
- Diferencial de código de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Los seis archivos preexistentes protegidos conservan exactamente sus SHA-256 registrados.
- Tipos nuevos declarados: 0.
- Recursión añadida: 0.

La validación fue exclusiva de la suite nueva. No se ejecutaron NSC, NSLink, las 684 pruebas históricas,
minibaselines, baseline, cobertura, sanitizadores, instalación ni despliegue. El resultado verificable es 4/5
casos de `CS-SRC-007`; `TS-SRC-049` permanece bloqueado y visible.

## Trazabilidad

| Caso | Símbolo observado | Oráculo |
|---|---|---|
| `TS-SRC-045` | `addBuffer`, `getFile` | IDs 1 y 2; snapshots correctos |
| `TS-SRC-046` | `addBuffer`, `getFile` | sin deduplicación; objetos distintos |
| `TS-SRC-047` | `getFile` | `nullptr` para cero y mayor que N |
| `TS-SRC-048` | `addBuffer`, `getFile` | identidad, path y bytes exactos |

Diagramas: no aplica. La tanda agrega casos a la arquitectura aprobada sin introducir flujo o dependencia de
producto nuevos.
