# Literales decimales de las expresiones reportadas

Base de la campana: 152654e5a0e2d84d4c2f245ea7590488f0497430. Caso COMP-02,
A:/native-sharp-reference/REPORTE-PARA-WORKER-COMPILADOR-SDK.md.

## Alcance
Corregir la perdida de precision de los literales de las expresiones reproducidas. No se
modifican A:/nsc-devel, ejemplos originales, C:/native-sharp, LLVM/Clang, Daikon ni la
interoperabilidad C/C++; no se usan sus builds ni se realiza merge, rebase, push o despliegue.
Builds, temporales y resultados permanecen en A. Los cambios que requieran decision de diseno
de lenguaje o API quedan pendientes. El baseline y la bateria completa pertinentes siguen
pendientes.

## Causa y correccion
El parser conservaba el lexema, pero HIR y MIR transmitian el literal como double; la generacion
reconstruia el decimal con formato de 17 digitos. Para 0.3m eso incorporaba el error binario.
Se conserva el texto del token decimal en el campo stringValue ya serializado en HIR/MIR y,
para literales de expresion, se obtienen el coeficiente y escala del texto exacto, incluido el
exponente. La ruta de valores sinteticos que no tienen token conserva el mecanismo anterior.
El coeficiente de esta ruta sigue limitado a 64 bits; no se afirma soporte completo de los
96 bits ni de inicializadores estaticos o de toda la aritmetica decimal.

## Verificacion
La prueba nsc.compiler.decimal-literal-exactness fallo antes y paso despues: cubre igualdad de
decimas, factura, redondeo y exponentes. La reproduccion historica copiada paso a informar
invoice cents=2314, Exact tenths=True y below=100 half=101 above=101; termino con codigo 0.
La misma prueba compilo y ejecuto en O0 y O2. Dos compilaciones O0 con cache aislada pasaron;
la segunda registro tres aciertos y cero fallos de cache. Tambien pasaron las regresiones
dictionary-string-update, decimal-generic-array y global-lambda con el compilador actualizado.
Evidencia en A:/nsc-fixes-evidencia/cases/comp-02/, decimal-cache/ y los logs
baseline-decimal-literal.log y fixed-decimal-literal-exponents.log.

La bateria completa, el analisis estatico integral y las rutas de decimal no mencionadas siguen
pendientes; estos resultados no prueban que la implementacion decimal completa sea correcta.

Analisis dirigido: clang-tidy con clang-analyzer-* sobre HirExpressionMachine.cpp y
OopLlvmExpressionLeaves.cpp termino sin errores; informo 8 y 9 avisos de padding,
respectivamente, en cabeceras y estructuras no modificadas. No se ejecuto linea base
comparable del analizador, por lo que no se afirma ausencia de regresiones estaticas.
