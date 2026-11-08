# Suma saturada de int

Base de la campana 152654e5a0e2d84d4c2f245ea7590488f0497430. Parte de COMP-01,
A:/native-sharp-reference/REPORTE-PARA-WORKER-COMPILADOR-SDK.md.

## Alcance
Corregir la suma int dentro de saturating en el ejemplo 48. No se modifican A:/nsc-devel,
los ejemplos originales, C:/native-sharp, LLVM/Clang, Daikon ni interoperabilidad C/C++;
no se usan sus builds ni se hace merge, rebase, push o despliegue. Builds, temporales y
evidencia quedan en A. Las decisiones de lenguaje o API pendientes no se implementan sin
decision. Las conversiones saturadas del mismo ejemplo, checked i128, el baseline y la
bateria completa pertinentes siguen pendientes.

## Causa y cambio
MIR conserva el wrapper saturating sobre un binary Plus int. La generacion de ese Plus ignoraba
la politica y emitia LLVMBuildAdd, que envuelve. Se usa el intrinseco LLVM de suma con
saturacion con signo solo para Plus de builtin:int bajo esa politica. La suma ordinaria no se
cambia. No se cambia la suma de otros tipos ni otras operaciones.

## Pruebas y limites
La prueba nsc.compiler.saturating-int-add fallo antes y paso despues: comprueba ambos limites,
una suma normal bajo saturating y la suma ordinaria con wrapping. Tambien compilo y ejecuto en
O2. En la reproduccion historica copiada, Int policy paso de -2147483648 a 2147483647;
Audio cast y Color cast siguen en -25536 y 34, respectivamente, y se trabajan por separado.
Evidencia: A:/nsc-fixes-evidencia/baseline-sat-add.log, fixed-sat-add-final.log,
sat-add-o2/ y cases/comp-01/after-add.json.

clang-tidy con clang-analyzer-* no informo errores y produjo nueve avisos de padding en
cabeceras no modificadas; no se ejecuto linea base comparable del analizador. No se afirman
verificados otros operadores, otros tipos ni la bateria completa.
