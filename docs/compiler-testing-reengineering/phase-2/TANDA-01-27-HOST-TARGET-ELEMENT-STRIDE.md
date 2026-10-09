# Tanda 01.27 — Stride de elementos de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Target` |
| Contrato | `CF-TGT-008`, cálculo de stride de elemento |
| Casos implementados | `TF-TGT-057..064` |
| Código de producto | Incluir y observar `TargetDataLayout::strideOf`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

La tanda cierra los ocho casos diseñados para `CF-TGT-008`, incluido el caso `NIGHTLY`. Verifica que el stride
sea la menor alineación válida no menor que el tamaño, y que todo error permanezca en el canal `nullopt`.

## Contratos ejercitados

- 1/1 produce stride 1 y tamaño 5 con alineación 4 produce 8;
- una instancia recién creada calcula 7/8 como 8 sin estado externo;
- tamaño cero, alineación cero y alineación no potencia de dos producen `nullopt`;
- una alineación cuya suma de máscara desborda también produce `nullopt`;
- máximo con alineación 1, máximo menos 15 con alineación 16 y alineación `2^63` permanecen exactos;
- una matriz de siete tamaños por siete alineaciones coincide con la referencia entera;
- orden inverso y repetición no cambian resultados;
- 65,536 elementos con tamaños `index * 29 + 1` y alineaciones 1..128 coinciden con la referencia y producen el
  checksum `62,277,115,904`.

La fórmula de referencia sólo se usa en entradas donde la suma es representable. El caso de overflow exige
`nullopt` directamente al producto y no calcula un valor envuelto en el oráculo.

## Diseño e integración

- `TargetElementStrideModule.cpp` registra ocho casos pequeños en el host consolidado.
- La matriz separa tamaños y alineaciones y compara cada combinación con aritmética independiente.
- El caso de escala es iterativo `O(N)`, usa memoria `O(1)` y no introduce recursión.
- El manifiesto conserva inputs y oráculos tipados; `TF-TGT-064` mantiene perfil `NIGHTLY`.
- `run_host_target_element_stride_gate.py` selecciona el contrato con perfil acumulativo `NIGHTLY` y exige
  exactamente `TF-TGT-057..064`.
- CMake reutiliza el ejecutable consolidado y no introduce un ejecutable por caso.

## Evidencia de validación

- Gate dirigido `CF-TGT-008`: 8/8 aprobado en Debug y Release, incluidos 7 casos `PR` y 1 `NIGHTLY`.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 26/26 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 26/26 aprobado.
- `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos.
- Compilación en memoria de los 47 archivos Python del controlador: 47/47 aprobada sin generar cachés.
- Manifiesto: 194 casos válidos, 0 IDs duplicados y 8 casos de `CF-TGT-008`.
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
| `TF-TGT-057` | elemento 1/1 | stride 1 |
| `TF-TGT-058` | elemento 5/4 | stride 8 |
| `TF-TGT-059` | elemento 7/8 | stride 8 sin estado externo |
| `TF-TGT-060` | cuatro elementos inválidos | `nullopt` |
| `TF-TGT-061` | límites de `uint64_t` | cuatro strides exactos |
| `TF-TGT-062` | matriz 7 por 7 | referencia exacta |
| `TF-TGT-063` | cinco elementos en dos órdenes | determinismo |
| `TF-TGT-064` | 65,536 elementos | referencia y checksum exactos |

Diagramas: no aplica. El contrato es una consulta numérica sin estado con canal opcional de error.
