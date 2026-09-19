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
