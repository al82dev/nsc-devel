# Tanda 01.26 — Alineación a potencia de dos de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Target` |
| Contrato | `CF-TGT-007`, alineación a potencia de dos |
| Casos implementados | `TF-TGT-049..056` |
| Código de producto | Observar `alignUp` por `strideOf`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

La tanda cierra los ocho casos diseñados para `CF-TGT-007`, incluido el caso `NIGHTLY`. `alignUp` es privado;
la prueba conserva su encapsulación y observa su contrato mediante `strideOf`, el consumidor público directo.

## Contratos ejercitados

- alinear 1 a 1 conserva 1;
- alinear 5 a 4 produce 8;
- una instancia recién creada alinea 3 a 2 como 4 sin estado externo;
- alineaciones 0, 3 y 6 producen `nullopt`;
- el consumidor público rechaza tamaño cero antes de alinear;
- máximo alineado a 1 conserva máximo;
- máximo menos 15 ya alineado a 16 permanece idéntico;
- máximo menos 14 alineado a 16 produce `nullopt` porque la suma de máscara desborda;
- una matriz de siete valores por siete potencias de dos coincide con la fórmula entera de referencia;
- `2^63`, la mayor potencia de dos representable en `uint64_t`, alinea 1 exactamente a `2^63`;
- repetición y orden inverso conservan resultados;
- 65,536 valores con ciclo de alineaciones 1..64 coinciden con la referencia y producen el checksum
  `36,508,340,804`.

La fórmula de referencia `((value + alignment - 1) / alignment) * alignment` sólo se aplica a entradas donde la
suma está demostrada representable. Los casos de overflow se evalúan únicamente por el canal `nullopt` del
producto, evitando que el propio oráculo envuelva.

## Diseño e integración

- `TargetPowerOfTwoAlignmentModule.cpp` registra ocho casos pequeños en el host consolidado.
- Un helper traduce tamaño/alineación a `strideOf`; no replica la implementación privada.
- La matriz y la escala usan aritmética de referencia independiente sobre dominios acotados.
- El caso de escala recorre iterativamente 65,536 entradas con memoria `O(1)`; no usa recursión.
- El manifiesto conserva inputs y oráculos tipados; `TF-TGT-056` mantiene perfil `NIGHTLY`.
- `run_host_target_power_of_two_alignment_gate.py` selecciona el contrato con perfil acumulativo `NIGHTLY` y
  exige exactamente `TF-TGT-049..056`.
- CMake reutiliza el ejecutable consolidado y no introduce un ejecutable por caso.

## Evidencia de validación

- Gate dirigido `CF-TGT-007`: 8/8 aprobado en Debug y Release, incluidos 7 casos `PR` y 1 `NIGHTLY`.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 25/25 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 25/25 aprobado.
- `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos.
- Compilación en memoria de los 46 archivos Python del controlador: 46/46 aprobada sin generar cachés.
- Manifiesto: 186 casos válidos, 0 IDs duplicados y 8 casos de `CF-TGT-007`.
- Dependencias dinámicas de ambos ejecutables: sólo `KERNEL32.dll`; importaciones de CRT dinámico: 0.
- Diferencial de código de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Los seis archivos preexistentes protegidos conservan exactamente sus SHA-256 registrados.
- Tipos nuevos declarados: 0.
- Recursión añadida: 0.

La validación fue exclusiva de la suite nueva. No se ejecutaron NSC, NSLink, las 684 pruebas históricas,
minibaselines, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Entrada observada por `strideOf` | Oráculo |
|---|---|---|
| `TF-TGT-049` | 1, alineación 1 | 1 |
| `TF-TGT-050` | 5, alineación 4 | 8 |
| `TF-TGT-051` | 3, alineación 2 | 4 sin estado externo |
| `TF-TGT-052` | 5, alineaciones 0/3/6 | `nullopt` |
| `TF-TGT-053` | cero y límites de `uint64_t` | exactitud y overflow |
| `TF-TGT-054` | matriz y potencia `2^63` | referencia entera exacta |
| `TF-TGT-055` | entradas repetidas en dos órdenes | determinismo |
| `TF-TGT-056` | 65,536 entradas | referencia y checksum exactos |

Diagramas: no aplica. El contrato es una transformación numérica sin estado con canal opcional de error.
