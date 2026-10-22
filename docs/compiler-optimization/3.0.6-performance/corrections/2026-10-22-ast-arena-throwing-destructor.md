# Corrección de destrucción excepcional en AstArena

## Causa

El cambio a almacenamiento por bloques sustituyó el `delete` del thunk de destrucción por
`std::destroy_at`. Con el toolchain MSVC usado por las pruebas, un destructor que lanza terminaba el
proceso dentro de esa ruta antes de alcanzar el límite de excepción de `AstArena::~AstArena()`.
Como consecuencia, tampoco se destruían las entradas anteriores del arena.

## Corrección

El thunk invoca explícitamente el destructor de `T`. La memoria continúa perteneciendo al bloque y
se libera al destruirlo, mientras que una excepción del destructor alcanza el `catch` ya existente y
permite continuar la limpieza en orden inverso.

No se modificaron pruebas, expectativas, sintaxis, ABI ni contratos del lenguaje.

## Validación focal

- Target: `nsc_component_tests_destructive` Debug.
- Caso: `TA-AST-016`.
- Resultado anterior: FAIL, salida del subproceso 225 después de `THROWING_DESTRUCTOR`.
- Resultado corregido: 1 PASS, 0 FAIL, salida 0.
- Evidencia: `A:\_codex-unit-cycle\20261022-focal-TA-AST-016-03`.
- No se ejecutó suite externa, minibaseline ni baseline.

