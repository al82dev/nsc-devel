# Tanda 01.29 — Layout de structs simples de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Target` |
| Contrato | `CF-TGT-010`, layout secuencial de `TypeLayout` |
| Casos implementados | `TF-TGT-073..080` |
| Código de producto | Incluir y observar `TargetDataLayout::layoutStruct(span<TypeLayout>)`; no modificar |
| Superficie excluida | Overload general, pack, union, offset explícito, flexible arrays y bit-fields |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

La tanda cierra los ocho casos diseñados para `CF-TGT-010`, incluido el caso `NIGHTLY`. Verifica offsets naturales,
padding interno y final, alineación agregada, preservación del orden y rechazo de campos o aritmética inválidos.

## Contratos ejercitados

- un struct de un byte produce tamaño 1, alineación 1 y offset cero;
- la secuencia 1/1, 4/4, 2/2 y 8/8 produce offsets 0, 4, 8 y 16, tamaño 24 y alineación 8;
- una instancia recién creada procesa un struct sin estado ni metadata externos;
- tamaño cero, alineación cero, alineación no potencia de dos, overflow de alineación y overflow del final de campo
  producen `nullopt`;
- el struct vacío es 1/1, dos campos repetidos conservan offsets distintos y los extremos representables permanecen
  exactos;
- las 400 combinaciones ordenadas de dos campos formadas por cuatro tamaños y cinco alineaciones coinciden con el
  oráculo secuencial independiente;
- el orden de invocación y la repetición no cambian ninguno de los campos del resultado;
- 65,536 campos producen tamaño 419,440, alineación 16 y último offset 419,424, mientras el caso cercano al máximo
  que desborda alineación produce `nullopt`.

## Oráculo independiente

El oráculo usa residuo y padding para alinear, en lugar de la suma de máscara y borrado de bits del producto. Antes
de cada suma comprueba el espacio restante de `uint64_t`; después conserva offsets, tamaños, alineaciones y metadata
opcional de todos los campos. El resultado sólo se considera igual si coinciden la cabecera y el vector completo.

El algoritmo es iterativo, `O(F)` en tiempo y `O(F)` en memoria por el resultado esperado. No introduce recursión.

## Diseño e integración

- `TargetPlainStructLayoutModule.cpp` registra ocho casos en el host consolidado.
- `run_host_target_plain_struct_layout_gate.py` selecciona perfil acumulativo `NIGHTLY` y exige exactamente
  `TF-TGT-073..080`.
- El manifiesto conserva entradas y oráculos tipados; `TF-TGT-080` mantiene perfil `NIGHTLY`.
- CMake reutiliza el ejecutable consolidado y no introduce un ejecutable por caso.
- Ningún caso cruza hacia el overload general ni intenta probar capacidades asignadas a contratos posteriores.

## Evidencia de validación

- Gate dirigido `CF-TGT-010`: 8/8 aprobado en Debug y Release, incluidos 7 casos `PR` y 1 `NIGHTLY`.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 28/28 aprobado en 25.91 s.
- Release: build con warnings como errores y CTest de la suite nueva 28/28 aprobado en 10.60 s.
- `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos.
- Compilación en memoria de los 49 archivos Python del controlador: 49/49 aprobada sin generar cachés.
- Manifiesto: 210 casos válidos, 0 IDs duplicados y 8 casos de `CF-TGT-010`.
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
| `TF-TGT-073` | un campo 1/1 | layout 1/1, offset cero |
| `TF-TGT-074` | cuatro campos representativos | offsets y padding exactos |
| `TF-TGT-075` | dos campos en una instancia nueva | resultado exacto sin prerrequisito externo |
| `TF-TGT-076` | cinco formas inválidas | `nullopt` |
| `TF-TGT-077` | vacío, repetidos y límites | layouts exactos |
| `TF-TGT-078` | 400 secuencias de dos campos | referencia secuencial completa |
| `TF-TGT-079` | cinco fixtures en dos órdenes | determinismo e igualdad completa |
| `TF-TGT-080` | 65,536 campos y overflow cercano al máximo | resumen exacto y rechazo acotado |

Diagramas: no aplica. El contrato es un recorrido secuencial sin estado y con un único canal opcional de error.
