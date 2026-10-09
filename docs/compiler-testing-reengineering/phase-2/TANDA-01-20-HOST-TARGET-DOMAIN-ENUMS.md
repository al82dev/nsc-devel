# Tanda 01.20 — Enumeraciones de dominio de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Target` |
| Contrato | `CF-TGT-001`, enumeraciones del dominio target |
| Casos implementados | `TF-TGT-001..008` |
| Código de producto | Incluir y observar `TargetInfo` y `TargetDataLayout`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

La tanda es independiente de los tres bloqueos conservados en `Host.Source` y abre `Host.Target` sin alterar el
producto. Cierra los ocho casos diseñados para `CF-TGT-001`, incluido su caso `NIGHTLY`.

## Contratos ejercitados

- los valores iniciales de `Architecture`, `OperatingSystem`, `ObjectFormat`, `CpuFeatureLevel` y
  `BuiltinLayoutKind` permanecen en ordinal cero;
- las identidades x64 y ARM64 producidas por `TargetInfo` conservan arquitectura, Windows y COFF;
- los getters de nivel CPU conservan exactamente el nivel solicitado y seleccionado;
- la inicialización por valor de los cinco enums no necesita metadata ni otro productor y produce su primer valor;
- un `BuiltinLayoutKind` no enumerado usa el fallback documentado `{size = 0, alignment = 1}`;
- los extremos ordinales son `Architecture 0..1`, `CpuFeatureLevel 0..3` y `BuiltinLayoutKind 0..17`;
- la matriz exhaustiva contiene 2 arquitecturas, 1 OS, 1 formato, 4 niveles CPU y 18 kinds de layout, todos en
  orden consecutivo;
- construcciones repetidas preservan las mismas tablas e identidades;
- 32,768 recorridos de los 25 valores nombrados producen el checksum independiente `5,242,880` y terminan sin
  estado acumulado ni recursión.

El caso inválido no realiza casts directos cuestionables. Construye el valor subyacente con `std::bit_cast`, que
conserva los bits del tipo base, y observa exclusivamente el fallback público implementado por `layoutOf`.

## Diseño e integración

- `TargetDomainEnumsModule.cpp` contiene constantes `constexpr` para las cinco tablas y ocho funciones de
  registro cohesionadas.
- `hasSequentialValues` comprueba el orden sin duplicar tablas de nombres ni depender de LLVM.
- El caso de escala acumula ordinales en un bucle iterativo y usa un resultado calculado fuera del producto.
- El manifiesto conserva el vocabulario cerrado del controlador; `TF-TGT-008` mantiene perfil `NIGHTLY`.
- `run_host_target_domain_enums_gate.py` selecciona `CF-TGT-001` con perfil acumulativo `NIGHTLY` y exige
  exactamente `TF-TGT-001..008`.
- CMake reutiliza el ejecutable consolidado e incorpora las dos implementaciones reales de Target necesarias;
  no crea un ejecutable por caso.

La primera ejecución del gate rechazó clases descriptivas que no pertenecían al vocabulario cerrado del
manifiesto. El análisis estático también rechazó casts directos fuera del conjunto nombrado. Ambos defectos estaban
en la prueba no versionada: se corrigieron antes del commit sin tocar producto ni relajar validadores.

## Evidencia de validación

- Gate dirigido `CF-TGT-001`: 8/8 aprobado en Debug y Release, incluidos 7 casos `PR` y 1 `NIGHTLY`.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 19/19 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 19/19 aprobado.
- `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos en código propio; 143 advertencias de
  dependencias o cabeceras del sistema fueron identificadas y suprimidas por la herramienta.
- Compilación en memoria de los 40 archivos Python del controlador: 40/40 aprobada sin generar cachés.
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
| `TF-TGT-001` | enums; getters de identidad x64 | ordinales cero e identidad exacta |
| `TF-TGT-002` | enums; getters ARM64 y CPU | ordinales 1, 2 y 17; identidad exacta |
| `TF-TGT-003` | cinco enums | inicialización por valor igual a cero |
| `TF-TGT-004` | `TargetDataLayout::layoutOf` | fallback exacto `0/1` |
| `TF-TGT-005` | cinco tablas | primeros y últimos ordinales exactos |
| `TF-TGT-006` | cinco tablas completas | cardinalidad y secuencia exhaustivas |
| `TF-TGT-007` | tablas; getters de `TargetInfo` | repetición idéntica |
| `TF-TGT-008` | cinco tablas completas | checksum iterativo `5,242,880` |

Diagramas: no aplica. La tanda incorpora un contrato de valores sin introducir flujo o dependencia de producto
nuevos.
