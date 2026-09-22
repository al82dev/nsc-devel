# Commit Tanda 49 / SH47

Estado: `STAGED-COMMITTED`; gate dirigido y baseline acumulado aprobados.

Título: `selfhost(tanda-49): close specialized code and NSAsm parity`

Incluye el lowering real de ensamblador inline mediante LLVM C, resolución nominal controlada de
operandos, contrato/verificador SH47, fixture NativeSharp y fixture NSAsm. Congela vectores,
matrices, Half, formatos empacados, SIMD, AVX/FMA, instrucciones packed y control de punto flotante.

Gate dirigido: 51/51 pruebas de infraestructura; 8/8 ejecuciones O0–O3 por LLD/NSLink; auditoría
de código máquina nativo y NSAsm; SH41 `nsc1→nsc2→nsc3` aprobado con 16 ejecuciones y punto fijo por
perfil. El fallo real de ruta larga encontrado por el primer baseline se corrigió con
`GetFullPathNameW` y uso consistente de la ruta normalizada; SH44 pasó sus cuatro variantes.
Baseline `tanda-49-sh47-specialized-code-r2-20260921`: 482/482 CTests, 148/148 fixtures y
18/18 casos de triaje por cada linker; cero fallos, 4638.981 s y Gate 0 `LANDED`.
SHA-256 del reporte:
`c72b382c8376525e402aed4dff9d18057d4d8f93dc3d56c002a6db3c72e0d9a0`.

No incluir `selfhost/out`, builds, objetos, DLL, EXE, LIB, RSP, logs ni temporales.
