# Contratos de fronteras del compilador autoalojado

- `llvm-c-api.contract.json` congela la DLL, versión, ABI, tipos opacos, hashes de enums y firmas,
  los 219 imports LLVM-C exactos de `nsc0` y los 42 símbolos ya expuestos al bootstrap NativeSharp.
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
