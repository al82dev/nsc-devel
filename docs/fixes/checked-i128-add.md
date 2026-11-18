# Suma `checked` de i128

El caso original calculaba `checked(maximo + uno)` y continuaba con wrapping.
Por decisión del usuario, el overflow debe detenerse con trap. La suma
comprobada de `i128` usa ahora el intrinsic de overflow firmado de LLVM y,
solo cuando desborda, llama al trap de overflow entero (motivo 19, salida 147).
El cálculo ordinario conserva su política anterior.

El ejemplo original compiló con el compilador aislado y terminó con código
147 en O0 y O2 (`A:/nsc-fixes-evidencia/checked-i128-initial/results.json`).
La regresión `nsc.compiler.checked-i128-add` comprueba en ambos niveles los
extremos positivo y negativo, sumas válidas en los límites y la suma ordinaria;
pasó (`full-suite-run/checked-tests.log`). El cambio cubre suma firmada i128:
resta, multiplicación, conversiones y otros anchos no quedan acreditados por
esta regresión ni se declaran corregidos aquí.
Control integrado adicional: el acarreo entre mitades de 64 bits y sumas con
signos opuestos pasaron en O0 y O2; los controles existentes de trap también
pasaron. Comando `A:/nsc-fixes-evidencia/numeric-targeted-20261111.cmd`, log
`numeric-i128-20261111.log`. El compilador tenía otros cambios locales y
NSLink se invocó por nombre como dependencia externa de solo lectura.
