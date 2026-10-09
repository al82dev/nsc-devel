# Grafo de componentes de NSC

## Estado y fuentes

Estado: `HECHO COMPROBADO` para targets, enlaces, referencias de proyecto e includes de la revisión
`3bb175443a18b2049c20d75443dd6743e2993903`.

Fuentes:

- `CMakeLists.txt:132-655`, targets de producto y enlaces;
- `CMakeLists.txt:670-891`, artefactos posteriores al enlace e instalación;
- `selfhost/nsc/NativeSharp.Compiler.nsproj`, fuentes y referencias self-host;
- todos los `#include "nativesharp/..."` de los 523 archivos C++ y headers versionados;
- los ocho proyectos `.nsproj` de `lib/**` y `selfhost/nsc/**`.

Los manifiestos canónicos viven en `inventory/build-targets.csv`, `source-target-map.csv`,
`target-dependencies.csv`, `include-dependencies.csv`, `native-projects.csv` y sus mapas asociados.

## Eje 1: composición física del host C++

| Productor | Consume | Fuentes | Responsabilidad física |
|---|---|---:|---|
| `nsc_build_identity` | plantilla `BuildId.h.in` | 0 | genera identidad de cada construcción |
| `nsc_basic` | identidad generada | 2 | transacciones de artefactos y versión |
| `nsc_compilation` | contratos header-only | 37 | lenguaje, proyectos y políticas de jobs |
| `nsc_target` | contratos header-only | 2 | target y data layout |
| `nsc_frontend` | `nsc_target`, `nsc_compilation` | 142 | frontend y representaciones intermedias |
| `nsc_language_service` | `nsc_frontend`, `nsc_basic` | 2 | fachada exportada para LSP |
| `nsc_llvm` | `nsc_frontend`, `nsc_target`, LLVM 23 | 18 | lowering, optimización y objetos |
| `nsc_runtime` | `nsc_llvm` | 3 | contrato ABI y biblioteca runtime Win32 |
| `nsc_linker` | `nsc_runtime`, `nsc_llvm` | 8 | construcción/verificación de productos PE/COFF |
| `nsc_driver` | linker, runtime, LLVM, frontend, target y basic | 29 | comandos, políticas y metadata pública |
| `nsc` | `nsc_driver` | 1 | ejecutable y punto de entrada |

La suma de fuentes host es 242. La comparación independiente entre `git ls-files src/**/*.cpp` y los bloques
`add_library`/`add_executable` encontró 242 mapeadas, cero faltantes y cero extras.

## Eje 2: componentes conceptuales por carpeta

El grafo de includes contiene 24 componentes host, 1,431 ocurrencias internas y 112 aristas distintas entre
componentes. Su condensación produce 17 grupos y un único grupo cíclico:

`Compilation; Driver; Frontend; Interop; Linker; LLVM; Parser; Runtime`

Esto no contradice el grafo de enlace acíclico. La carpeta conceptual no siempre determina el target físico:
`CompilationJobPolicy.cpp` y `CompilationWorkerPool.cpp`, por ejemplo, viven bajo `Driver` pero se compilan en
`nsc_compilation`. Por ello:

- los test plans usan el componente conceptual para responsabilidad y contrato;
- la futura implementación usa el target físico para fixtures, linkage y aislamiento;
- ninguna prueba se declara unitaria sólo porque un archivo esté en una carpeta determinada;
- una dependencia circular por headers se documenta, no se “corrige” durante esta fase.

El orden acíclico previo al grupo conectado es:

1. `Basic`, `Language`, `Target`;
2. `Source`;
3. `Diagnostics`;
4. `Lexer`;
5. `AST`;
6. `Sema`;
7. `HIR`;
8. `MIR`;
9. `CFG`;
10. `Dominance`;
11. `SSA`;
12. `Passes`;
13. grupo conectado de ocho componentes;
14. `LanguageService` y `nsc`.

## Eje 3: compilador self-host

`NativeSharpCompilerNext` contiene 27 fuentes `.ns` y referencia cuatro proyectos:

- `NativeSharp.Core`;
- `NativeSharp.Core.Collections.Generic`;
- `NativeSharp.Core.IO`;
- `NativeSharp.LLVM.C`.

Los ocho proyectos NativeSharp inventariados contienen 119 fuentes, todas asociadas exactamente a un proyecto.
El grafo completo tiene nueve referencias explícitas. Las generaciones self-host añaden una dependencia de
productor: una imagen anterior válida de NSC produce la siguiente; los contratos de bootstrap determinan cuándo
esa salida tiene autoridad para alimentar equivalencia, fixed point y determinismo.

## Fronteras para la suite nueva

1. **Unitarias puras:** valores, parsers pequeños, clasificadores y estructuras sin productor externo.
2. **Componente:** contratos internos de un target o de un grupo conceptual conectado.
3. **Integración de pipeline:** transiciones Lexer → AST → Sema → HIR → MIR → CFG → SSA → Passes → LLVM.
4. **Producto:** comandos del driver, artefactos, runtime y enlace mediante el `nsc` construido.
5. **Self-host:** generaciones, equivalencia y fixed point; nunca sustituyen las unitarias diarias.

La Tanda 02 sólo fija fronteras. Los casos concretos se diseñan después de inventariar símbolos y contratos.
