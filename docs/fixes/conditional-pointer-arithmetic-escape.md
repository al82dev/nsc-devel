# Escape de punteros condicionales con desplazamiento

Una selección entre una dirección global y una local, seguida de un desplazamiento y entregada a una biblioteca NativeSharp con contrato de escape desconocido, se aceptaba indebidamente. La reproducción se analizó sin ejecutar la llamada insegura. `conditional-pointer-arithmetic-before.log` registra un fallo de 34 comprobaciones para `(false ? &Data : &local) + 0`; una primera corrección dejó fallando `0 + (false ? &Data : &local)`, como muestra `conditional-pointer-arithmetic-reverse-before.log` (un fallo de 35).

La clasificación de valores de puntero sigue el operando de tipo puntero en sumas y restas antes de reunir las procedencias de las ramas condicionales. Si no existe información del tipo del segundo operando, analiza ambos conservadoramente; esto puede producir un rechazo adicional, en lugar de perder una dirección local. El análisis existente de dependencias entre funciones no cambia.

La compilación incremental y la regresión `conditional-pointer-arithmetic-after.cmd` pasaron 35 comprobaciones. El control incluye ambos órdenes de suma y resta con direcciones globales y desplazamiento local. `conditional-pointer-arithmetic-controls.cmd` pasó las tres pruebas dirigidas de escape y vida de préstamos, con 35, 40 y 31 comprobaciones. Los comandos y registros están en `A:/nsc-fixes-evidencia`.

El compilador de prueba se construyó desde el árbol de trabajo con cambios locales aún pendientes. Estas pruebas no acreditan todos los tipos de expresión, todas las bibliotecas binarias ni la campaña completa; siguen pendientes los contratos de vistas compuestas en bibliotecas precompiladas.
