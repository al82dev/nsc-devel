# T08 H3: llamada C en inicializador de for

El control positivo añade un `for` cuya variable local se inicializa mediante
la función C homónima. Comprueba que el importador conserve la dependencia de
esa llamada y que el consumidor precompilado produzca 183 en O0–O3.

La compilación y el resultado se registrarán por separado; el alcance queda
sujeto a la semántica real de inicialización del compilador NativeSharp.
