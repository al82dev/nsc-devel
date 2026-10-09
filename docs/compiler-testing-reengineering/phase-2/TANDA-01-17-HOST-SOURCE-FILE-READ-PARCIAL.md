# Tanda 01.17 — Lectura acotada de archivos de Host.Source

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Source` |
| Contrato | `CS-SRC-006`, lectura binaria exacta y publicación atómica |
| Casos implementados | `TS-SRC-034..039`, `TS-SRC-042..044` |
| Casos bloqueados | `TS-SRC-040..041` |
| Código de producto | Incluir y observar `SourceManager` y `SourceFile`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

La tanda es independiente de la saturación bloqueada en `TS-SRC-008`. No cambia el cursor causal primario ni
presenta `CS-SRC-006` como cerrado: implementa nueve de sus once casos y conserva visibles los dos restantes.

## Contratos ejercitados

- un archivo vacío produce un `optional` presente con cero bytes;
- los bytes binarios, incluidos NUL y `0xff`, se conservan sin traducción de texto;
- 64 MiB exactos son un límite inclusivo y cada byte cero del fixture se conserva;
- 64 MiB más un byte se rechazan antes de leer y producen el diagnóstico exacto del límite;
- una ruta inexistente produce el diagnóstico exacto de apertura con su spelling real;
- un named pipe abre como stream pero no expone tamaño de archivo regular, por lo que ejercita el error de
  `file_size` sin permisos ambientales ni carreras;
- una lectura exitosa no borra un `errorMessage` preexistente, conforme al contrato vigente;
- una carga fallida no consume identidad ni publica un `SourceFile` parcial;
- una carga exitosa asigna el primer `FileId` y congela exactamente path y bytes.

## Diseño y algoritmos de prueba

`SourceFileReadModule.cpp` registra una función pequeña por caso. El registro público sólo compone esas nueve
funciones; ninguna supera 60 líneas ejecutables. Los helpers tienen responsabilidades separadas:

- `writeBytes` crea un fixture binario y comprueba el fallo final del stream;
- `resizeFixture` obtiene los límites de 64 MiB sin reservar un segundo buffer en el proceso de prueba;
- `containsOnlyZeroBytes` recorre iterativamente el resultado en `O(n)` y `O(1)` espacio adicional;
- `namedPipePath` crea un nombre único por proceso y reloj monotónico de Windows;
- `createNamedPipe` conserva el handle mediante RAII y lo libera al terminar el caso.

Los archivos temporales viven debajo del directorio de build en `A:`. `CaseTree` elimina de forma no recursiva
únicamente los paths que creó y registró. Los dos límites usan como máximo 64 MiB más un byte en disco; sólo el
caso de éxito materializa 64 MiB en memoria porque ése es precisamente el contrato público observado.

No se copia el algoritmo de `SourceManager`, no se accede a miembros privados y no se simula ningún retorno.
Cada oráculo compara estado opcional, bytes, identidad o diagnóstico observable.

## Bloqueos conservados

### `TS-SRC-040` — truncamiento después de `file_size`

La API pública entra directamente de `std::filesystem::file_size` a `ifstream::read` sin checkpoint, callback,
reloj, evento ni seam de inyección. Un thread que trunque por temporización sólo probaría una carrera del host y
no garantizaría que la mutación ocurriera después del tamaño y antes de la lectura.

Estado: `BLOQUEADO_POR: ausencia de coordinación determinista observable en la frontera pública`.

### `TS-SRC-041` — crecimiento después de `file_size`

La misma frontera no permite coordinar una escritura exactamente entre la consulta de tamaño y el `peek` final.
Una espera, polling o archivo grande no convierte el orden en determinista. Interceptar internals de `/MT`,
alterar layout o añadir un seam a producto violaría el `SCOPE LOCK`.

Estado: `BLOQUEADO_POR: ausencia de coordinación determinista observable en la frontera pública`.

No se añadieron entradas `SKIP`, tests vacíos, carreras probabilísticas, hooks de CRT ni cambios de producto.

## Integración

- `SourceFileReadModule.cpp` registra los nueve casos ejecutables.
- `SourceManager.cpp` se compila dentro del único ejecutable host sin modificar su fuente.
- `cases.jsonl` conserva entradas, oráculos, perfiles y tags de cada caso realmente ejecutable.
- `run_host_source_file_read_gate.py` selecciona `CS-SRC-006` con perfil acumulativo `WEEKLY` y exige la lista
  exacta de nueve IDs en orden.
- CMake conserva un único ejecutable y añade un gate de controlador; no crea un ejecutable por prueba.

## Errores encontrados durante la validación

La primera ejecución del controlador rechazó las etiquetas `negative` y `state` porque no pertenecen al
vocabulario cerrado del manifiesto. Se corrigieron exclusivamente a `invalid` y `valid-variant` antes de volver
a ejecutar. No se cambió el oráculo ni el comportamiento de ningún caso.

La primera pasada de `clang-tidy` encontró tres defectos en código nuevo:

1. multiplicación en `unsigned int` antes de ampliar a `uintmax_t`;
2. combinación bitwise de flags de stream marcada por `bugprone-signed-bitwise`;
3. tres parámetros `string_view` adyacentes e intercambiables en un helper.

La constante se hizo ancha desde el primer operando, el `ofstream` utiliza su truncamiento implícito y las tres
comparaciones se colocaron directamente en sus casos. La segunda pasada quedó sin hallazgos en código propio.

## Evidencia de validación

- Gate dirigido `CS-SRC-006`: 9/9 aprobado en Debug y Release.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 16/16 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 16/16 aprobado.
- `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: segunda pasada con 0 hallazgos en código propio; 284
  advertencias de dependencias o cabeceras del sistema fueron identificadas y suprimidas por la herramienta.
- Compilación en memoria del gate Python: aprobada sin generar cachés.
- Dependencias dinámicas de ambos ejecutables: sólo `KERNEL32.dll`; importaciones de CRT dinámico: 0.
- Diferencial de código de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Los seis archivos preexistentes protegidos conservan exactamente sus SHA-256 registrados.
- Líneas mayores a 120 caracteres en los archivos C++ y Python nuevos: 0.
- Tipos nuevos declarados: 0; se reutilizan los RAII ya verificados de la suite.
- Recursión añadida: 0.

La validación fue exclusiva de la suite nueva. No se ejecutaron NSC, NSLink, las 684 pruebas históricas,
minibaselines, baseline, cobertura, instalación ni despliegue. El resultado verificable es 9/11 casos de
`CS-SRC-006`; `TS-SRC-040..041` permanecen bloqueados y visibles.

## Trazabilidad

| Caso | Símbolo observado | Oráculo |
|---|---|---|
| `TS-SRC-034` | `readFileBuffer` | `optional` presente, bytes vacíos, error vacío |
| `TS-SRC-035` | `readFileBuffer` | bytes binarios exactos |
| `TS-SRC-036` | `readFileBuffer` | 64 MiB aceptados y todos los bytes preservados |
| `TS-SRC-037` | `readFileBuffer` | `nullopt` y diagnóstico exacto de límite |
| `TS-SRC-038` | `readFileBuffer` | `nullopt` y diagnóstico exacto de apertura |
| `TS-SRC-039` | `readFileBuffer` | `nullopt` y diagnóstico exacto de tamaño |
| `TS-SRC-042` | `readFileBuffer` | éxito sin modificar el error previo |
| `TS-SRC-043` | `loadFile` | fallo sin consumir `FileId` |
| `TS-SRC-044` | `loadFile` | `FileId{1}`, path y bytes exactos |

Diagramas: no aplica. La tanda añade casos a la arquitectura aprobada y no introduce módulos, dependencias ni
flujos de producto nuevos.
