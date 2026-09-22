# Commit Tanda 50 / SH48

Estado: `STAGED-COMMITTED`; gate dirigido y baseline acumulado aprobados.

Título: `selfhost(tanda-50): rebuild the complete library and Win32 runtime`

Incluye emisión de biblioteca desde el grafo fuente completo, soporte backend descubierto por las
80 unidades reales, binding y verificación LLVM por función, contrato/verificador SH48 y consumidor
de runtime. Produce objetos y archivos COFF deterministas en O0–O3, audita cuatro grupos ABI y
ejecuta 8/8 combinaciones con LLD y NSLink.

Los baselines intermedios descubrieron y cerraron dos defectos reales: callbacks `extern C delegate`
que incorporaban el allocator administrado, e inline asm cuya firma usaba un tipo aproximado. SH41,
SH46, SH47 y SH48 se repitieron después de las correcciones.

Baseline `tanda-50-sh48-runtime-library-r4-20260921`: 483/483 CTests, 148/148 fixtures y
18/18 casos de triaje por cada linker; cero fallos, 7832.537 s y Gate 0 `LANDED`.
SHA-256 del reporte:
`8346586ad620f64fa29ac5956eccd1e8f1f3bccce19ca3552e91e0c4b2c4d17a`.

No incluir `selfhost/out`, builds, objetos, DLL, EXE, LIB, RSP, logs ni temporales.
