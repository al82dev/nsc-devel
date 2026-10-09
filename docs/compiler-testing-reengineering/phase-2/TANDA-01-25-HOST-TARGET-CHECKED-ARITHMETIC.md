# Tanda 01.25 — Aritmética comprobada de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Target` |
| Contrato | `CF-TGT-006`, suma y multiplicación comprobadas de `uint64_t` |
| Casos implementados | `TF-TGT-041..048` |
| Código de producto | Observar `checkedAdd` y `checkedMultiply` por consumidores públicos; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

La tanda cierra los ocho casos diseñados para `CF-TGT-006`, incluido el caso `NIGHTLY`. Los helpers comprobados
son privados: la prueba conserva su encapsulación y observa suma mediante `layoutStruct` y multiplicación mediante
`layoutArray`, los consumidores públicos mínimos que propagan exactamente su éxito o `nullopt`.

## Contratos ejercitados

- las operaciones mínimas `0 + 1` y `1 * 1` producen 1;
- `32 + 10` y `6 * 7` producen 42;
- una instancia recién creada ejecuta ambas rutas sin estado externo;
- `uint64_t::max() + 2` y `uint64_t::max() * 2` producen `nullopt`;
- sumar cero a máximo en ambos órdenes conserva máximo;
- máximo por uno conserva máximo y `(máximo / 2) * 2` produce máximo menos uno;
- cinco pares representativos coinciden con aritmética de referencia independiente;
- repetición y permutación de operandos conservan resultados exactos;
- 65,536 iteraciones validan cuatro operaciones cada una: una suma y multiplicación representables, más una
  suma y multiplicación que deben desbordar;
- el checksum de resultados aceptados es `53,687,910,400` y ningún resultado aceptado envuelve.

El adaptador de suma expresa `left + right` como el extremo de un campo explícito. Para `right == 0` usa el
estado flexible permitido por el producto, lo que hace observable el límite `max + 0` sin introducir un campo
inválido. El adaptador de multiplicación expresa `left * right` como tamaño de array.

## Diseño e integración

- `TargetCheckedArithmeticModule.cpp` registra ocho casos pequeños en el host consolidado.
- Dos helpers de prueba traducen operaciones a APIs públicas sin replicar los checks de overflow.
- La matriz calcula resultados esperados mediante aritmética constante previamente demostrada representable.
- El caso de escala acumula resultados durante un recorrido iterativo `O(N)` y memoria `O(1)`; no usa recursión.
- El manifiesto conserva inputs y oráculos tipados; `TF-TGT-048` mantiene perfil `NIGHTLY`.
- `run_host_target_checked_arithmetic_gate.py` selecciona el contrato con perfil acumulativo `NIGHTLY` y exige
  exactamente `TF-TGT-041..048`.
- CMake reutiliza el ejecutable consolidado y no introduce un ejecutable por caso.

## Evidencia de validación

- Gate dirigido `CF-TGT-006`: 8/8 aprobado en Debug y Release, incluidos 7 casos `PR` y 1 `NIGHTLY`.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 24/24 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 24/24 aprobado.
- `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos.
- Compilación en memoria de los 45 archivos Python del controlador: 45/45 aprobada sin generar cachés.
- Manifiesto: 178 casos válidos, 0 IDs duplicados y 8 casos de `CF-TGT-006`.
- Dependencias dinámicas de ambos ejecutables: sólo `KERNEL32.dll`; importaciones de CRT dinámico: 0.
- Diferencial de código de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Los seis archivos preexistentes protegidos conservan exactamente sus SHA-256 registrados.
- Tipos nuevos declarados: 0.
- Recursión añadida: 0.

La validación fue exclusiva de la suite nueva. No se ejecutaron NSC, NSLink, las 684 pruebas históricas,
minibaselines, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Ruta pública observada | Oráculo |
|---|---|---|
| `TF-TGT-041` | struct y array mínimos | suma y producto iguales a 1 |
| `TF-TGT-042` | struct y array representativos | suma y producto iguales a 42 |
| `TF-TGT-043` | ambas rutas en valor nuevo | uso sin estado externo |
| `TF-TGT-044` | ambas rutas con máximo por 2 | `nullopt` por overflow |
| `TF-TGT-045` | ambas rutas en límites | máximo, cero, uno y casi máximo exactos |
| `TF-TGT-046` | ambas rutas, cinco pares | referencia independiente exacta |
| `TF-TGT-047` | ambas rutas repetidas y permutadas | determinismo y conmutatividad observable |
| `TF-TGT-048` | ambas rutas, 262,144 operaciones | aceptación, rechazo y checksum exactos |

Diagramas: no aplica. El contrato es aritmética sin estado con un canal opcional de overflow.
