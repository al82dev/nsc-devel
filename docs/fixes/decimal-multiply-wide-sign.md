# Multiplicación decimal: coeficiente y signo

La reproducción dirigida demostró que la ruta LLVM extraía solo los 64 bits inferiores
de un coeficiente decimal de 96 bits y no incorporaba el signo al producto normal. En O0 y
O2, un coeficiente `2^64` multiplicado por uno y un producto negativo devolvían resultados
incorrectos; `Max × 2` no producía el trap requerido y tampoco saturaba correctamente.
El registro anterior a la corrección está en
`A:/nsc-limitaciones-numericos-evidencia/baseline-mul-div/observations.txt`.

La rama de multiplicación extrae los 96 bits de cada operando y calcula su producto exacto
en 192 bits mediante desplazamiento y suma, en hasta 96 iteraciones; termina cuando se
agotan los bits del multiplicador. Compara el producto completo con el máximo de 96 bits
antes de truncar. Conserva la guarda previa de escala y la política existente: overflow
default/checked produce `DecimalOverflow` y saturating devuelve el límite con signo XOR.
Los resultados normales también reciben el signo XOR. No cambia las ramas add/sub/div/rem
ni las declaraciones en la cabecera compartida. El costo del bucle no se ha medido y el
signo binario del cero no se ha verificado como contrato independiente.

El target `nsc` se construyó incrementalmente en el build aislado. El ejecutable usado,
SHA-256 `a8f04352b005f57b5d913cf65972f126165d9dc7b9d629d63b72850c8b8a74b0`, compiló y
enlazó los siete controles en O0 y O2: signo, coeficiente alto, escala, overflow checked,
ambos límites saturados y producto superior a 128 bits; 14 ejecuciones tuvieron el resultado
esperado. El control de literales y factura también pasó O0/O2. Comandos y resultados de
la primera matriz: `A:/nsc-limitaciones-numericos-evidencia/fix-multiply/observations.txt`.
El ejecutor autónomo `VerifyDecimalMultiply.cmake` pasó la misma matriz en
`A:/nsc-limitaciones-numericos-evidencia/fix-multiply-runner/observations.txt`.
La compilación y enlace de esos casos no demuestran ausencia general de helpers en otras rutas.

El análisis dirigido de `OopLlvmOperators.cpp` con `clang-tidy` y `clang-analyzer-*` terminó
0 con nueve avisos de padding en cabeceras no modificadas; registro
`A:/nsc-limitaciones-numericos-evidencia/static-multiply.log`. No se dispone de una línea
base comparable del analizador. Las pruebas son dirigidas y no certifican toda la aritmética
decimal. Restricciones generales: `TAREAS-CHAT-NUMERICOS.md`; la integración en el registro
CMake compartido corresponde al integrador central.
