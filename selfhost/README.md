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

Las generaciones tienen nombres normativos `nsc0`, `nsc1`, `nsc2` y `nsc3`. SH10 materializa
`nsc1` dos veces desde raíces limpias independientes y exige identidad byte a byte. Ningún
directorio de salida puede compartirse entre generaciones.

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

SH08 incorpora el primer backend LLVM escrito en NativeSharp. Construye módulo, tipos, globales,
funciones, bloques, aritmética y llamadas Win64 mediante LLVM-C 23, aplica O0–O3, emite objeto y
assembly y certifica ejecución con `lld-link` y NSLink.

SH09 añade la primera superficie de proyecto: `.nsproj`, referencias, response files, metadata
N-1/N, claves de caché y manifiestos deterministas; cuatro perfiles enlazan con ambos linkers.

SH10 congela el conjunto fuente de `nsc1`, lo construye con `nsc0` en dos rutas absolutas, audita
PE/imports y ejecuta con el propio `nsc1` el corpus de los subsistemas y O0–O3 con ambos linkers.

SH11 añade una transición autónoma y auditable de `nsc1` a `nsc2`: el productor valida con su
propio frontend e IR las fuentes congeladas, no crea procesos hijos y reproduce la imagen
certificada en dos raíces y entornos distintos. Esta frontera se denomina explícitamente
`source-attested-image-replay`; todavía no es codegen derivado de las fuentes y no autoriza por sí
sola retirar la semilla C++.

SH12 repite `nsc2 → nsc3` tres veces sin normalizar artefactos y exige identidad byte a byte. La
prueba local exporta una atestación portable; el gate permanece abierto hasta importar una
atestación coincidente de otra máquina Windows x64.

SH13 añade presupuestos medidos, fuzzing determinista, metadata hostil, fallo atómico de salida,
interrupción y auditoría de temporales sobre las imágenes locales `nsc2`/`nsc3`.

SH14 automatiza la decisión de promoción. Mientras algún gate requerido esté abierto o no exista
codegen derivado de fuentes, genera una selección segura con `nsc0` predeterminado y `nsc2`
candidato, conservando rollback explícito.

Tanda 16 sella el estado de los gates y convierte los pendientes restantes en un ledger
ejecutable. El cierre administrativo conserva la no promoción y no altera los contratos SH12–14.
