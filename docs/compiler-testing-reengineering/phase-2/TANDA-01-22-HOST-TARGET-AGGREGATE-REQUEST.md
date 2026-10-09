# Tanda 01.22 — Modelo de solicitud de agregados de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Target` |
| Contrato | `CF-TGT-003`, modelo de solicitud de layout de agregados |
| Casos implementados | `TF-TGT-017..024` |
| Código de producto | Incluir y observar `TargetDataLayout.h` y usar `TargetDataLayout::layoutStruct`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

La tanda cierra los ocho casos diseñados para `CF-TGT-003`, incluido el caso `NIGHTLY`. Verifica el modelo público
que describe un agregado antes de calcular su layout. No cambia ni duplica reglas del producto.

## Contratos ejercitados

- `StructLayoutOptions` por defecto describe un `struct` no unión, sin `pack` ni alineación mínima impuesta;
- `StructFieldRequest` inicializado por valor conserva layout cero, optionals vacíos y banderas falsas;
- ambos registros preservan exactamente valores representativos suministrados por el consumidor;
- construir los registros no necesita ningún productor o estado externo;
- los registros son valores pasivos, mientras `TargetDataLayout::layoutStruct` conserva la responsabilidad de
  rechazar combinaciones inválidas;
- `pack == 3`, un bitfield con offset explícito y un bitfield marcado como arreglo flexible son rechazados por
  el productor real;
- cero, uno y `uint64_t::max()` se conservan sin truncamiento en los campos aplicables;
- la matriz distingue campos planos, offset y alineación explícitos, arreglos flexibles, bitfields y barreras
  anónimas de ancho cero, además de opciones plain, union, pack y alineación mínima;
- copiar ambos registros conserva igualdad campo por campo;
- 65,536 solicitudes preservan cardinalidad, primer, medio y último offset, junto con el checksum independiente
  `17,179,738,112`.

`TF-TGT-020` separa deliberadamente dos responsabilidades. No espera que los registros pasivos validen sus
campos; entrega las combinaciones al `layoutStruct` real y exige que ese productor las rechace. Así la prueba no
inventa validación dentro de los valores ni replica el algoritmo del producto.

## Diseño e integración

- `TargetAggregateRequestModule.cpp` registra ocho casos pequeños en el host consolidado.
- Los casos de valores observan únicamente la superficie pública de los registros.
- El caso inválido llama al productor real con tres entradas concretas y exige ausencia de resultado.
- El caso de escala reserva una sola vez, llena iterativamente y calcula el checksum durante la construcción;
  complejidad `O(R)` en tiempo y memoria, sin recursión.
- El manifiesto conserva inputs y oráculos tipados; `TF-TGT-024` mantiene perfil `NIGHTLY`.
- `run_host_target_aggregate_request_gate.py` selecciona el contrato con perfil acumulativo `NIGHTLY` y exige
  exactamente `TF-TGT-017..024`.
- CMake reutiliza el ejecutable consolidado y no introduce un ejecutable por caso.

## Evidencia de validación

- Gate dirigido `CF-TGT-003`: 8/8 aprobado en Debug y Release, incluidos 7 casos `PR` y 1 `NIGHTLY`.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 21/21 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 21/21 aprobado.
- `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos en código propio; 143 advertencias de
  dependencias o cabeceras del sistema fueron identificadas y suprimidas por la herramienta.
- Compilación en memoria de los 42 archivos Python del controlador: 42/42 aprobada sin generar cachés.
- Dependencias dinámicas de ambos ejecutables: sólo `KERNEL32.dll`; importaciones de CRT dinámico: 0.
- Diferencial de código de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Los seis archivos preexistentes protegidos conservan exactamente sus SHA-256 registrados.
- Tipos nuevos declarados: 0.
- Recursión añadida: 0.

La validación fue exclusiva de la suite nueva. No se ejecutaron NSC, NSLink, las 684 pruebas históricas,
minibaselines, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Símbolo observado | Oráculo |
|---|---|---|
| `TF-TGT-017` | ambos registros | defaults exactos |
| `TF-TGT-018` | ambos registros | valores representativos campo por campo |
| `TF-TGT-019` | ambos registros | construcción sin productor externo |
| `TF-TGT-020` | `TargetDataLayout::layoutStruct` | rechazo de tres combinaciones inválidas |
| `TF-TGT-021` | ambos registros | límites cero, uno y máximo |
| `TF-TGT-022` | ambos registros | matriz de variantes de campo y opciones |
| `TF-TGT-023` | ambos registros | copias deterministas campo por campo |
| `TF-TGT-024` | `StructFieldRequest` | 65,536 solicitudes y checksum exacto |

Diagramas: no aplica. La tanda verifica registros de solicitud y rechazo dirigido sin introducir flujo o
dependencia de producto nuevos.
