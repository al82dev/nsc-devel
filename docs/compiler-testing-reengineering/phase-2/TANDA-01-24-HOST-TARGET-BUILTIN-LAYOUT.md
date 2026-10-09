# Tanda 01.24 — Tabla builtin de layout de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Target` |
| Contrato | `CF-TGT-005`, tabla builtin de layouts escalares |
| Casos implementados | `TF-TGT-033..040` |
| Código de producto | Incluir y observar `TargetDataLayout::layoutOf`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

La tanda cierra los ocho casos diseñados para `CF-TGT-005`, incluido el caso `NIGHTLY`. Verifica de forma aislada
la tabla completa de `layoutOf` y su canal documentado para valores desconocidos.

## Contratos ejercitados

- `Bool`, `SByte`, `Byte` y `Char` producen 1/1;
- `Short` y `UShort` producen 2/2;
- `Int`, `UInt`, `Float` y `Enum` producen 4/4;
- `Long`, `ULong`, `NInt`, `NUInt`, `Double` y `Pointer` producen 8/8;
- `Int128` y `UInt128` producen 16/16;
- una instancia recién creada responde sin metadata, registro o productor externo;
- los valores subyacentes desconocidos `-1`, `18` y `255` producen el fallback `{0,1}`;
- los extremos representables del tipo subyacente también producen `{0,1}`;
- consultar la tabla en orden directo, inverso o repetido no cambia ningún resultado;
- 65,536 consultas sobre el ciclo de 18 entradas producen el checksum exacto `12,116,864`.

Los valores desconocidos se construyen con `std::bit_cast` desde el tipo subyacente. Esto observa el fallback
público sin depender de una conversión fuera del rango válido del enum.

## Diseño e integración

- `TargetBuiltinLayoutModule.cpp` registra ocho casos pequeños en el host consolidado.
- Dos tablas `constexpr` mantienen separados inputs y oráculos exactos para las 18 entradas.
- Los casos inválido y frontera prueban exclusivamente el fallback documentado.
- El caso determinista conserva resultados directos e inversos en arreglos fijos y vuelve a consultar cada kind.
- El caso de escala recorre iterativamente 65,536 entradas con memoria `O(1)`; no usa recursión.
- El manifiesto conserva inputs y oráculos tipados; `TF-TGT-040` mantiene perfil `NIGHTLY`.
- `run_host_target_builtin_layout_gate.py` selecciona el contrato con perfil acumulativo `NIGHTLY` y exige
  exactamente `TF-TGT-033..040`.
- CMake reutiliza el ejecutable consolidado y no introduce un ejecutable por caso.

## Evidencia de validación

- Gate dirigido `CF-TGT-005`: 8/8 aprobado en Debug y Release, incluidos 7 casos `PR` y 1 `NIGHTLY`.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 23/23 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 23/23 aprobado.
- `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos.
- Compilación en memoria de los 44 archivos Python del controlador: 44/44 aprobada sin generar cachés.
- Manifiesto: 170 casos válidos, 0 IDs duplicados y 8 casos de `CF-TGT-005`.
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
| `TF-TGT-033` | `layoutOf(Bool)` | 1/1 exacto |
| `TF-TGT-034` | `layoutOf` representativo | Int 4/4, Pointer 8/8 e Int128 16/16 |
| `TF-TGT-035` | `layoutOf(Byte)` | uso sin estado externo y 1/1 exacto |
| `TF-TGT-036` | `layoutOf` desconocido | fallback 0/1 para tres valores |
| `TF-TGT-037` | extremos conocidos y subyacentes | layouts exactos y fallback 0/1 |
| `TF-TGT-038` | las 18 entradas | tabla completa exacta |
| `TF-TGT-039` | las 18 entradas | independencia del orden y repetición |
| `TF-TGT-040` | las 18 entradas en ciclo | 65,536 consultas y checksum exacto |

Diagramas: no aplica. El contrato es una función total sobre valores conocidos con un fallback estable.
