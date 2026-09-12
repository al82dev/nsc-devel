# 240 — Compiladores host herméticos / Hermetic host compilers

## Resultado / Outcome

**ES.** Las pruebas ABI de hosts C y C++ usan las rutas absolutas de `clang` y `clang++` resueltas
por CMake. El baseline puede aislar `PATH` sin convertir una prueba del contrato binario en un fallo
de búsqueda de ejecutable.

**EN.** C and C++ host ABI tests use the absolute `clang` and `clang++` paths resolved by CMake. The
baseline may isolate `PATH` without turning a binary-contract test into an executable lookup
failure.

## Causa / Cause

Stage 47, 48.1, 48.3, 48.4 y 48.6 construían comandos que comenzaban literalmente con `clang` o
`clang++`. La construcción encontraba LLVM mediante rutas configuradas, pero los procesos de prueba
volvían a depender del ambiente interactivo. Bajo el entorno hermético ambos nombres dejaban de
existir aunque `C:\clang-llvm\bin` estuviera instalado.

Stages 47, 48.1, 48.3, 48.4, and 48.6 built commands starting with literal `clang` or `clang++`.
The build found LLVM through configured paths, but test processes depended again on the interactive
environment. Under the hermetic environment both names disappeared even though
`C:\clang-llvm\bin` was installed.

## Implementación / Implementation

- CMake busca obligatoriamente los dos drivers con hints de LLVM y la instalación contractual.
- Cada target recibe sólo la ruta que utiliza mediante una definición de compilación.
- Los comandos citan la ruta absoluta antes de agregar sus argumentos y redirecciones.
- Stage 48.6 deja de omitir silenciosamente el probe: si se construyó el target, el compilador
  configurado forma parte de su contrato.

CMake requires both drivers using LLVM and contractual-installation hints. Each target receives
only the path it uses, commands quote the absolute executable, and Stage 48.6 no longer silently
skips the probe after its configured compiler was required at configure time.

## Archivos / Files

- `CMakeLists.txt`
- `tests/unit/Stage47HostCAbiTests.cpp`
- `tests/unit/Stage48_1CHeaderGeneratorTests.cpp`
- `tests/unit/Stage48_3CHostIntegrationTests.cpp`
- `tests/unit/Stage48_4CppHostIntegrationTests.cpp`
- `tests/unit/Stage48_6CppCliHostClosureTests.cpp`
- `mkdown/240_compiladores_host_hermeticos.md`

## Verificación dirigida / Targeted verification

Se reconstruyen y ejecutan únicamente los cinco tests de host afectados en el entorno actual. Los
cinco probes de Clang pasan; Stage 48.6 conserva una aserción independiente de versión histórica,
atendida por la entrega siguiente. La entrega termina con una reconstrucción limpia del compilador;
no ejecuta baseline.

Only the five affected host tests are rebuilt and executed in the current environment. All five
Clang probes pass; Stage 48.6 retains an independent historical-version assertion handled by the
next delivery. The delivery ends with a clean compiler rebuild and does not run a baseline.

## Commit de referencia / Reference commit

**Título ES:** Hace herméticas las pruebas de hosts C y C++

**Title EN:** Make C and C++ host tests hermetic

**Descripción ES:** Resuelve clang y clang++ durante la configuración e inyecta sus rutas absolutas
en las pruebas ABI que antes dependían de PATH.

**Description EN:** Resolve clang and clang++ at configure time and inject their absolute paths into
ABI tests that previously depended on PATH.
