# Tanda 01.23 — Fábricas de layout Windows de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Target` |
| Contrato | `CF-TGT-004`, fábricas de layout Windows |
| Casos implementados | `TF-TGT-025..032` |
| Código de producto | Incluir y observar `TargetDataLayout.h` y `TargetDataLayout.cpp`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

La tanda cierra los ocho casos diseñados para `CF-TGT-004`, incluido el caso `NIGHTLY`. Demuestra que
`WindowsX64()` y `WindowsArm64()` publican el mismo modelo de datos Windows LLP64 que posee actualmente
`TargetDataLayout`, sin atribuirles identidad de arquitectura ni responsabilidades del backend.

## Contratos ejercitados

- ambas fábricas producen valores sin estado externo y utilizables inmediatamente;
- ambas publican little-endian y alineación natural máxima igual a 8;
- la tabla completa de 18 `BuiltinLayoutKind` coincide con los tamaños y alineaciones documentados;
- punteros son 8/8 y enteros de 128 bits son 16/16 en ambas fábricas;
- `strideOf`, `layoutArray`, `layoutStruct` simple y `layoutStruct` general conservan paridad completa;
- un struct vacío produce tamaño 1, alineación 1 y ningún campo sin requerir prerrequisitos;
- tamaño cero, count cero y campos con alineación cero son rechazados de manera idéntica;
- un array de un byte por `uint64_t::max()` elementos permanece representable, mientras el equivalente de dos
  bytes rechaza overflow;
- fábricas repetidas producen propiedades, escalares y agregados idénticos;
- 65,536 consultas de array sobre el ciclo completo de tipos builtin conservan paridad y el checksum exacto
  `14,557,107,568`.

La fábrica ARM64 no se prueba contra calling conventions o clasificación ABI porque esa responsabilidad no
pertenece a `TargetDataLayout`. La prueba se limita al contrato declarado: layouts escalares, de puntero y de
agregados iguales a Windows x64.

## Diseño e integración

- `TargetWindowsFactoriesModule.cpp` registra ocho casos pequeños en el host consolidado.
- Dos tablas `constexpr` enumeran las 18 entradas y sus oráculos independientes de tamaño/alineación.
- Helpers sin estado comparan resultados de tipo, array, campo y struct sin modificar el producto.
- El caso inválido somete las mismas tres entradas a ambas fábricas y exige el mismo rechazo.
- El caso de escala realiza un recorrido iterativo `O(N)` con memoria `O(1)` y un checksum fijo; no usa
  recursión ni conserva resultados masivos.
- El manifiesto conserva inputs y oráculos tipados; `TF-TGT-032` mantiene perfil `NIGHTLY`.
- `run_host_target_windows_factories_gate.py` selecciona el contrato con perfil acumulativo `NIGHTLY` y exige
  exactamente `TF-TGT-025..032`.
- CMake reutiliza el ejecutable consolidado y no introduce un ejecutable por caso.

## Evidencia de validación

- Gate dirigido `CF-TGT-004`: 8/8 aprobado en Debug y Release, incluidos 7 casos `PR` y 1 `NIGHTLY`.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 22/22 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 22/22 aprobado.
- `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos.
- Compilación en memoria de los 43 archivos Python del controlador: 43/43 aprobada sin generar cachés.
- Manifiesto: 162 casos válidos, 0 IDs duplicados y 8 casos de `CF-TGT-004`.
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
| `TF-TGT-025` | ambas fábricas y propiedades | endian, alineación máxima y layout mínimo exactos |
| `TF-TGT-026` | ambas fábricas y cuatro familias | modelo LLP64 representativo y paridad |
| `TF-TGT-027` | ambas fábricas, struct vacío | uso sin estado externo y resultado exacto |
| `TF-TGT-028` | ambas fábricas, tres consultas inválidas | rechazo idéntico |
| `TF-TGT-029` | ambas fábricas, arrays frontera | mínimo, máximo representable y overflow |
| `TF-TGT-030` | ambas fábricas, superficie completa | tabla builtin y cinco familias de consulta |
| `TF-TGT-031` | cuatro instancias | repetición determinista |
| `TF-TGT-032` | ambas fábricas, 65,536 consultas | paridad y checksum exacto |

Diagramas: no aplica. Las fábricas son productores sin estado y no introducen flujo o jerarquía nuevos.
