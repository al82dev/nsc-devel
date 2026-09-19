# NativeSharp 2.0 self-hosting

Este árbol contiene la implementación NativeSharp del compilador y la infraestructura que demuestra
su bootstrap. El compilador C++ permanece fuera de este árbol como `nsc0`, semilla y oráculo
temporal.

## Layout congelado por SH00

- `bootstrap/`: locks, contratos, verificadores, limpieza y orquestación.
- `nsc/`: fuentes de `nsc-next`, organizadas por fronteras del compilador.
- `bindings/`: fronteras NativeSharp hacia LLVM C y Win32.
- `tests/`: corpus diferencial y pruebas exclusivas del bootstrap.
- `out/`: builds, trabajo y temporales; siempre generado e ignorado por Git.
- `evidence/runs/`: evidencia generada por corrida; no se mezcla con fuentes.

Las generaciones tienen nombres normativos `nsc0`, `nsc1`, `nsc2` y `nsc3`. Ningún directorio de
salida puede compartirse entre generaciones.

NSLink y NSAsm no se reescriben dentro de este repositorio. Una reimplementación escrita en
NativeSharp debe vivir en un repositorio nuevo; este proyecto sólo consume herramientas externas
mediante contratos versionados.

## Frontera alcanzada

SH05 incorpora en `nsc/` el lexer, el árbol sintáctico congelable y el parser escritos en
NativeSharp. `nsc-next --dump-tokens` y `nsc-next --dump-ast` son superficies diferenciales
versionadas por `contracts/frontend.contract.json`; no invocan ni cargan el frontend C++.

SH06 añade tablas nominales de tipos y símbolos, scopes, layout Windows x64, binding de
expresiones, constantes, conversiones, operadores, lambdas y metadatos semánticos deterministas.
`--dump-symbols`, `--dump-sema` y `--dump-semantic-metadata` son superficies autocontenidas. El
contrato `contracts/semantic.contract.json` exige igualdad con `nsc0` y estabilidad O0/O2 sobre
todo el corpus fuente.

SH07 incorpora HIR versionado, lowering a MIR, CFG, dominancia y SSA escritos en NativeSharp.
Cada frontera tiene verificador propio y dump determinista; `contracts/ir.contract.json` compara
funciones e invariantes contra `nsc0` y exige estabilidad exacta de O0, O1, O2 y O3. El perfil de
pases hace explícito qué análisis y transformaciones pertenecen a cada nivel.
