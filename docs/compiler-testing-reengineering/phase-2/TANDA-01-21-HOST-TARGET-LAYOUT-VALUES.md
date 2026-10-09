# Tanda 01.21 — Registros de valor de layout de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Target` |
| Contrato | `CF-TGT-002`, registros de valor de layout |
| Casos implementados | `TF-TGT-009..016` |
| Código de producto | Incluir y observar `TargetDataLayout.h`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

La tanda cierra los ocho casos diseñados para `CF-TGT-002`, incluido el caso `NIGHTLY`. No cambia reglas de
layout: verifica únicamente el contrato de los cuatro registros públicos que transportan sus resultados.

## Contratos ejercitados

- `TypeLayout` preserva `size` y `alignment` sin transformación;
- `ArrayLayout` preserva `size`, `alignment`, `stride` y `count`;
- `StructFieldLayout` preserva offset, tamaño, alineación y los estados independientes de `bitOffset` y
  `bitWidth`;
- `StructLayout` preserva tamaño, alineación, orden de campos y ownership del vector;
- la inicialización agregada por valor produce escalares cero, optionals vacíos y vector vacío sin productores
  externos;
- los registros son pasivos: combinaciones incoherentes se almacenan exactamente y no se validan ni normalizan;
- cero, uno y `uint64_t::max()` permanecen sin truncamiento;
- los cuatro estados relevantes de metadata —vacío, ambos optionals, sólo width y valores máximos— se preservan;
- una copia de `StructLayout` posee un vector independiente y mantiene orden y metadata tras mutar el original;
- un `StructLayout` con 65,536 campos conserva cardinalidad, primer, medio y último offset, además del checksum
  independiente `17,179,607,040`.

La aparente entrada inválida de `TF-TGT-012` no espera rechazo. El contrato declara `errors: none`: estos tipos son
valores sin constructor validador. Rechazar tamaños o alineaciones corresponde a `TargetDataLayout`, no a los
registros. La prueba evita atribuirles una política que el producto no posee.

## Diseño e integración

- `TargetLayoutValuesModule.cpp` registra ocho casos pequeños en el host consolidado.
- Cada caso usa inicialización agregada pública; no accede a estado privado ni duplica algoritmos de layout.
- El caso de ownership modifica el origen después de copiar para demostrar separación real del vector.
- El caso de escala reserva una sola vez, llena iterativamente y mueve el vector al resultado; complejidad
  `O(F)` en tiempo y memoria, sin recursión.
- El manifiesto conserva inputs y oráculos tipados; `TF-TGT-016` mantiene perfil `NIGHTLY`.
- `run_host_target_layout_values_gate.py` selecciona el contrato con perfil acumulativo `NIGHTLY` y exige
  exactamente `TF-TGT-009..016`.
- CMake reutiliza el ejecutable consolidado y no introduce un ejecutable por caso.

## Evidencia de validación

- Gate dirigido `CF-TGT-002`: 8/8 aprobado en Debug y Release, incluidos 7 casos `PR` y 1 `NIGHTLY`.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 20/20 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 20/20 aprobado.
- `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos en código propio; 143 advertencias de
  dependencias o cabeceras del sistema fueron identificadas y suprimidas por la herramienta.
- Compilación en memoria de los 41 archivos Python del controlador: 41/41 aprobada sin generar cachés.
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
| `TF-TGT-009` | cuatro registros | valores mínimos campo por campo |
| `TF-TGT-010` | cuatro registros | valores representativos y bit metadata |
| `TF-TGT-011` | cuatro registros | inicialización por valor |
| `TF-TGT-012` | tres registros | preservación pasiva de estado incoherente |
| `TF-TGT-013` | cuatro registros | cero, uno, máximo y vector vacío |
| `TF-TGT-014` | `StructFieldLayout`, `StructLayout` | matriz de optionals y orden |
| `TF-TGT-015` | `StructLayout` | copia profunda del vector |
| `TF-TGT-016` | `StructLayout` | 65,536 campos y checksum exacto |

Diagramas: no aplica. La tanda valida valores y ownership sin introducir flujo o dependencia de producto nuevos.
