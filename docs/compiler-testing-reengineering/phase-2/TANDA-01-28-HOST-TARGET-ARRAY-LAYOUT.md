# Tanda 01.28 — Layout de arreglos de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Target` |
| Contrato | `CF-TGT-009`, cálculo del layout de arreglos |
| Casos implementados | `TF-TGT-065..072` |
| Código de producto | Incluir y observar `TargetDataLayout::layoutArray`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

La tanda cierra los ocho casos diseñados para `CF-TGT-009`, incluido el caso `NIGHTLY`. Verifica el stride,
la alineación, el conteo y el tamaño total del arreglo, además del canal `nullopt` para entradas inválidas o
resultados que no caben en `uint64_t`.

## Contratos ejercitados

- un elemento 1/1 con conteo 1 produce tamaño, alineación, stride y conteo iguales a 1;
- un elemento 5/4 con conteo 3 produce tamaño 24, alineación 4, stride 8 y conteo 3;
- una instancia recién creada calcula 7/8 por dos elementos sin estado externo;
- conteo cero, tamaño cero, alineación cero, alineación no potencia de dos y multiplicación con overflow producen
  `nullopt`;
- los límites representables de `uint64_t` permanecen exactos y máximo por dos se rechaza;
- una matriz de cuatro tamaños, cinco alineaciones y cuatro conteos coincide campo por campo con una referencia
  aritmética independiente;
- orden inverso y repetición no cambian resultados;
- 65,536 arreglos con ciclos independientes de tamaño, alineación y conteo coinciden con la referencia y producen
  el checksum `83,156,681`.

La referencia calcula primero el stride con división y multiplicación enteras y sólo multiplica por el conteo en
entradas acotadas. Los casos de overflow exigen `nullopt` directamente al producto, sin construir un oráculo
envuelto.

## Diseño e integración

- `TargetArrayLayoutModule.cpp` registra ocho casos pequeños en el host consolidado.
- El oráculo compara los cuatro campos de `ArrayLayout`, no únicamente el tamaño agregado.
- La matriz y el caso de escala son iterativos; usan memoria acotada y no introducen recursión.
- El manifiesto conserva entradas y oráculos tipados; `TF-TGT-072` mantiene perfil `NIGHTLY`.
- `run_host_target_array_layout_gate.py` selecciona el contrato con perfil acumulativo `NIGHTLY` y exige
  exactamente `TF-TGT-065..072`.
- CMake reutiliza el ejecutable consolidado y no introduce un ejecutable por caso.

## Evidencia de validación

- Gate dirigido `CF-TGT-009`: 8/8 aprobado en Debug y Release, incluidos 7 casos `PR` y 1 `NIGHTLY`.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 27/27 aprobado en 24.00 s.
- Release: build con warnings como errores y CTest de la suite nueva 27/27 aprobado en 10.42 s.
- `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos.
- Compilación en memoria de los 48 archivos Python del controlador: 48/48 aprobada sin generar cachés.
- Manifiesto: 202 casos válidos, 0 IDs duplicados y 8 casos de `CF-TGT-009`.
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
| `TF-TGT-065` | elemento 1/1, conteo 1 | cuatro campos iguales a 1 |
| `TF-TGT-066` | elemento 5/4, conteo 3 | tamaño 24, alineación 4, stride 8, conteo 3 |
| `TF-TGT-067` | elemento 7/8, conteo 2 | resultado exacto sin estado externo |
| `TF-TGT-068` | cinco entradas inválidas | `nullopt` |
| `TF-TGT-069` | límites de `uint64_t` | tres resultados exactos y un overflow rechazado |
| `TF-TGT-070` | matriz 4 por 5 por 4 | referencia exacta campo por campo |
| `TF-TGT-071` | cinco elementos en dos órdenes | determinismo |
| `TF-TGT-072` | 65,536 arreglos | referencia y checksum exactos |

Diagramas: no aplica. El contrato es una consulta numérica sin estado con canal opcional de error.
