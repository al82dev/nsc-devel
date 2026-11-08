# Verificación de objetos en CTest

Once pruebas de emisión de objetos terminaron con error de CTest aunque el
compilador retornó cero. Sus expresiones exigían la salida técnica que
confirma `machine=0x8664`, pero el driver actual solo la muestra con
`--verbose`. Los once objetos de la primera ejecución también se inspeccionaron
directamente: cada uno tenía cabecera COFF AMD64 y al menos una sección.

Se añadió `--verbose` a esas once invocaciones, conservando intacta la
expresión que comprueba la máquina y sin cambiar el compilador. La selección
completa de 24 verificaciones LLVM y de objetos pasó 24/24 después de
reconfigurar CTest. La primera pasada, la inspección de cabeceras y la segunda
pasada están en `A:/nsc-fixes-evidencia/full-suite-run/` (`object-tests.log`,
`object2-tests.log` y los objetos en la carpeta de build aislada).
