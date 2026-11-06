# Funciones C/C++ globales desde namespace

La captura incluye el ámbito global. Si no existen candidatos en el namespace
actual ni en los importados, la resolución considera funciones C/C++ globales;
las funciones globales NativeSharp no cambian de visibilidad. Falta ejecutar
la matriz y los controles de prioridad en la validación agrupada.
