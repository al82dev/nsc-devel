# Division i128 con signo sin __divti3

Base de la campana: 152654e5a0e2d84d4c2f245ea7590488f0497430. Caso COMP-04,
A:/native-sharp-reference/REPORTE-PARA-WORKER-COMPILADOR-SDK.md.

## Alcance
Corregir la division i128 con signo del ejemplo 49. No se modifican A:/nsc-devel, los
ejemplos originales, C:/native-sharp, LLVM/Clang, Daikon ni interoperabilidad C/C++; no se
usan sus builds ni se hace merge, rebase, push o despliegue. Builds, temporales y evidencia
permanecen en A. Cambios que requieran decision de lenguaje o API permanecen pendientes.
Baseline y bateria completa pertinentes siguen pendientes.

## Causa y solucion
El backend emitia sdiv i128 y el enlazador requeria __divti3, ausente en la configuracion.
Solo para division con signo i128, despues de la guarda de cero y la correccion existente de
MinValue / -1, se emite division binaria restauradora: se transforman los operandos en
magnitudes, se recorre el numerador de bit alto a bajo, se mantiene resto y cociente y se
resta el divisor cuando corresponde, preservando el bit de acarreo; al final se restaura el
signo. Son exactamente 128 iteraciones, tiempo O(128) y estado O(1), sin recursividad,
asignacion dinamica ni dependencia de __divti3. Otras divisiones y el resto no se cambian.

## Evidencia
nsc.compiler.signed-i128-division fallaba al enlazar y pasa despues. Comprueba cocientes
grandes, signos, cociente cero, unidad, MinValue / -1, MinValue / 2 y trap de divisor cero
129. La reproduccion original copiada compila y retorna 0. Una comparacion independiente
de 23 pares contra resultados enteros de Python compilo y ejecuto en O0 y O2; divisor cero
retorno 129 en ambos modos. Los logs estan en A:/nsc-fixes-evidencia/baseline-i128-div.log,
fixed-i128-div-final.log, i128-div-differential/, i128-div-zero/ y cases/comp-04/.

clang-tidy clang-analyzer-* sobre OopLlvmOperators.cpp no informo errores. En comparacion
con el analisis anterior del mismo archivo se conservaron los mismos nueve avisos de padding
en cabeceras y no aparecieron avisos nuevos. La bateria general y el analisis de todo el
alcance siguen pendientes. No se afirma correccion del resto i128 ni de division sin signo.
