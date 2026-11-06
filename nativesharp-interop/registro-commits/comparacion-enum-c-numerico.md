# Comparación numérica de enums C importados

El análisis reconoce la representación de enums C completos al compararlos
con números; HIR selecciona el tipo numérico común de NativeSharp e inserta
conversiones desde la identidad enum. Los enums nativos conservan sus reglas y
no se amplían conversiones de asignación. La matriz de metadata C pasó de O0 a
O3 con comparaciones de enum compartido, enum estrecho con signo y enum de 32
bits con base unsigned explícita. La suite `nsc.cpp-interop.session` pasó. Esto
no demuestra todas las conversiones ni cierra la tanda. También pasó la prueba
existente `nsc.stage51.enum-oop-support`, que comprueba, entre otros casos, que
un enum NativeSharp comparado con un entero no nulo sigue rechazándose. No
acredita todos los usos o conversiones de enums nativos.
