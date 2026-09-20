# Contratos de fronteras del compilador autoalojado

- `llvm-c-api.contract.json` congela la DLL, versión, ABI, tipos opacos, hashes de enums y firmas,
  los 219 imports LLVM-C exactos de `nsc0` y los 44 símbolos ya expuestos al bootstrap NativeSharp.
- `runtime-bootstrap.contract.json` identifica los artefactos externos del runtime, Runtime ABI 1.0,
  symbol set 2 y los trece símbolos mínimos que debe conservar el bootstrap.

Una ampliación de cualquiera de estas fronteras exige cambiar el contrato, su fuente, su prueba y
el expediente de la tanda en el mismo commit. Los artefactos generados viven en `selfhost/out`; los
contratos son entradas versionadas.

`diagnostics.map.json` congela desde SH04 la identidad y el código de salida de cada diagnóstico
portado. El texto humano se compara byte a byte contra `nsc0`; la salida estructurada conserva el
código y normaliza sólo los campos que el contrato declara no semánticos.

`frontend.contract.json` congela tokens, árboles y recuperación de SH05.
`semantic.contract.json` congela desde SH06 símbolos y hechos semánticos diferenciales, estabilidad
O0/O2 de todo el corpus e invariantes de genéricos, layout, ensamblador y lambdas.
`ir.contract.json` congela desde SH07 las cinco fronteras IR, los cuatro niveles O0/O1/O2/O3,
el orden de pases y los mínimos del corpus que debe alcanzar SSA sin divergencia.

`llvm-backend.contract.json` congela desde SH08 el target LLVM 23/Win64, los perfiles O0–O3,
los artefactos objeto/assembly, símbolos y relocations mínimos, el código observable 42 y la
certificación cruzada con `lld-link` y NSLink.

`project-driver.contract.json` congela desde SH09 la carga `.nsproj`, orden de fuentes,
referencias, perfiles, response files, metadata N-1/N, manifiestos y enlace dual.

`first-generation.contract.json` congela desde SH10 las fuentes, dependencias, imports, perfiles
y linkers que definen el primer `nsc1` construido exclusivamente por `nsc0`.

`image-replay-retirement.contract.json` congela desde SH15 la prohibición de copiar la imagen
productora, el código 78 de bloqueo y los campos mínimos de la traza emitida mientras todavía no
existe codegen del compilador derivado de fuentes.

`source-derived-backend.contract.json` congela desde SH16 la primera rebanada real: dos fuentes,
resultados 17/42, O0/O3, `lld-link`/NSLink y desigualdad obligatoria entre hashes de objetos.

`source-functions.contract.json` congela desde SH17 funciones `int`, un máximo bootstrap de cuatro
parámetros, llamadas directas, O0–O3, los símbolos fuente obligatorios y ejecución con ambos
linkers. El límite de cuatro parámetros es de la rebanada, no de NativeSharp ni de LLVM.

`source-control-flow.contract.json` congela desde SH18 locales `int`, asignación simple,
comparaciones con signo, `if` y `while`, con la matriz O0–O3 y enlace dual.

`source-loop-control.contract.json` congela desde SH19 `for`, `do`/`while`, `break` y `continue`,
con pila de destinos, matriz O0–O3 y enlace dual.

`source-bool-void.contract.json` congela desde SH20 funciones, parámetros y locales `bool`, retornos
`void`, negación, comparaciones como valores y enlace dual O0–O3.

`source-expressions.contract.json` congela desde SH21 asignaciones compuestas, incremento y
decremento, resto, bitwise, shifts, `&&`/`||` con cortocircuito y selección condicional en O0–O3 y
ambos linkers.

`source-integer-widths.contract.json` congela desde SH22 anchos de 8/16/32/64 bits, `nint`/`nuint`,
conversiones contextuales y de operandos, signedness de división/resto/comparación/shift y enlace
dual O0–O3.
