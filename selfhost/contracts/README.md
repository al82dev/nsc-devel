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
