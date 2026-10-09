# Tanda 01.30 — Opciones y agregado vacío de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Target` |
| Contrato | `CF-TGT-011`, validación de opciones y agregado vacío |
| Casos implementados | `TF-TGT-081..088` |
| Código de producto | Incluir y observar `TargetDataLayout::layoutStruct(span<StructFieldRequest>, options)`; no modificar |
| Superficie excluida | Colocación de campos, pack efectivo, union, offsets, flexible arrays y bit-fields |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

La tanda cierra los ocho casos diseñados para `CF-TGT-011`, incluido el caso `NIGHTLY`. Verifica de forma aislada
la aceptación de opciones cero o potencia de dos, el rechazo de opciones inválidas y el contrato exacto de un
agregado sin campos.

## Contratos ejercitados

- opciones predeterminadas producen tamaño 1, alineación 1 y vector vacío;
- union con pack 8 y mínimo agregado 16 produce un agregado vacío 16/16;
- una instancia recién creada procesa pack 2 y mínimo 4 sin estado externo;
- pack o mínimo agregado igual a 3 o a `uint64_t::max()` produce `nullopt`;
- `2^63`, la mayor potencia de dos representable, se acepta como pack y como alineación mínima;
- la matriz cartesiana de union, seis packs y seis mínimos valida exactamente 72 combinaciones;
- orden inverso y repetición no cambian el resultado;
- 65,536 combinaciones cíclicas conservan resultados exactos y checksum `2,097,152`, y la opción máxima impar
  permanece rechazada.

Pack e indicador de union no alteran el resultado cuando no existen campos. La alineación mínima cero se
normaliza al contrato 1/1; cualquier potencia de dos positiva produce tamaño y alineación iguales a ese valor.

## Diseño e integración

- `TargetAggregateOptionsModule.cpp` registra ocho casos en el host consolidado.
- Los casos sólo entregan spans vacíos, por lo que no solapan contratos posteriores de colocación de campos.
- La matriz y el caso de escala son iterativos, `O(N)` y de memoria constante; no introducen recursión.
- El manifiesto conserva entradas y oráculos tipados; `TF-TGT-088` mantiene perfil `NIGHTLY`.
- `run_host_target_aggregate_options_gate.py` exige exactamente `TF-TGT-081..088`.
- CMake reutiliza el ejecutable consolidado y no introduce un ejecutable por caso.

## Evidencia de validación

- Gate dirigido `CF-TGT-011`: 8/8 aprobado en Debug y Release, incluidos 7 casos `PR` y 1 `NIGHTLY`.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 29/29 aprobado en 27.73 s.
- Release: build con warnings como errores y CTest de la suite nueva 29/29 aprobado en 11.12 s.
- `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos.
- Compilación en memoria de los 50 archivos Python del controlador: 50/50 aprobada sin generar cachés.
- Manifiesto: 218 casos válidos, 0 IDs duplicados y 8 casos de `CF-TGT-011`.
- Dependencias dinámicas de ambos ejecutables: sólo `KERNEL32.dll`; importaciones de CRT dinámico: 0.
- Diferencial de código de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Los seis archivos preexistentes protegidos conservan exactamente sus SHA-256 registrados.
- Tipos nuevos declarados: 0.
- Recursión añadida: 0.

La validación fue exclusiva de la suite nueva. No se ejecutaron NSC, NSLink, las 684 pruebas históricas,
minibaselines, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Entrada | Oráculo |
|---|---|---|
| `TF-TGT-081` | agregado vacío y opciones cero | 1/1 sin campos |
| `TF-TGT-082` | union, pack 8, mínimo 16 | 16/16 sin campos |
| `TF-TGT-083` | instancia nueva, pack 2, mínimo 4 | 4/4 sin estado externo |
| `TF-TGT-084` | cuatro opciones no potencia de dos | `nullopt` |
| `TF-TGT-085` | cero y `2^63` | tres resultados frontera exactos |
| `TF-TGT-086` | 72 combinaciones | matriz completa exacta |
| `TF-TGT-087` | seis fixtures en dos órdenes | determinismo |
| `TF-TGT-088` | 65,536 combinaciones | exactitud, checksum y rechazo límite |

Diagramas: no aplica. El contrato es una validación de opciones seguida de una salida vacía sin estado.
