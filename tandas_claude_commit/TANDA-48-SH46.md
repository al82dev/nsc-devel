# Commit Tanda 48 / SH46

Estado: `STAGED-COMMITTED`; baseline completo aprobado y commit preparado en la pila aislada.

Título: `selfhost(tanda-48): close native ABI and interop parity`

Incluye el ABI `extern C` de agregados pequeños, promociones variádicas, dirección correcta de
agregados, contrato, verificador, fixtures C/C++/COM/DLL, gate CMake, evidencia y expediente.

Gate dirigido: SH41 O0–O3 LLD/NSLink; cuatro productores autocontenidos; 64/64 ejecuciones SH46 y
47/47 pruebas de infraestructura. Baseline `tanda-48-sh46-abi-interop-r2-20260921`: 480/480 CTests,
148/148 fixtures y 18/18 casos de triage por enlazador; Gate 0 `LANDED`; 4030.470 s; cero fallos.
No incluir `selfhost/out`, builds, objetos, DLL, EXE, LIB ni logs.

Antes del commit: ejecutar `git diff --check`, revisar que sólo entren fuentes, contrato, pruebas,
evidencia, expediente y Gate 0; comprobar árbol limpio tras el commit.
