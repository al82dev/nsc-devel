# Límites de Exp y Log en Core

`DeterministicMath.Exp` devolvía el máximo double para entradas mayores que
709, aunque `709.1` tiene un resultado finito menor. También se observó
`Exp(-745)` como cero. Los registros originales están en
`A:/native-sharp-reference/validation/ncss-auditoria/sondeo-exp-709-2026-11-09/resultados.json`
y `A:/native-sharp-reference/validation/ncss-auditoria/sondeo-exp-subnormal-2026-11-09/resultados.json`.

El cambio local ajusta los límites de Exp, conserva el escalado normal antes
del producto subnormal y evita que Log normalice infinitamente el infinito
positivo. `tests/integration/deterministic_math_boundaries/Program.ns` verifica
los límites y controles de Log; el proyecto consume Core como fuente mediante
ProjectReference.

La ejecución dirigida de la copia de trabajo usó el compilador aislado
`A:/nsc-fixes-evidencia/build/bin/nsc.exe` y el runtime generado en A, con
temporales también en A y directorio de trabajo del repositorio. Los cuatro
cruces precise/deterministic y O0/O2 compilaron y devolvieron 0. Comandos,
salidas y hashes antes/después están en
`A:/nsc-fixes-evidencia/deterministic-math-directed-20261112T112737331804Z/results.json`.
El compilador tuvo SHA-256
`e6b7ea532d92443d1f0e5900b9de1a426e5c5fbb81b367416e789aa824d157f4`;
el código SDK y el fixture coinciden en hash con la evidencia previa
`A:/native-sharp-reference/validation/ncss-auditoria/verificacion-correccion-math-2026-11-09/cmake-subnormal-result.json`.

El build del compilador reutiliza objetos y contiene cambios locales; no es
una revisión limpia certificada. La prueba ejercita Core fuente, no consumo de
Core binario, y no acredita precisión para todas las entradas, denormales,
plataformas ni otros modos. La ejecución actual no cubrió O1/O3 ni CTest.
