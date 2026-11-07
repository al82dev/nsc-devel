# T08-004: validacion de global C estatica int

La construccion incremental con NSC termino con codigo 0 y advertencias de
inicializadores en el codigo existente. `pruebas-tanda-08-global-static/validar.cmd`
paso el control C y las variantes NativeSharp con y sin funciones inline en
O0, O1, O2 y O3. La regresion `pruebas-tanda-08-inline-c/validar.cmd` paso para
consumo directo y productores precompilados en los cuatro niveles.

El control nuevo se incorpora al gate de la tanda, que no se ha ejecutado para
esta modificacion. No se acredita por estos controles la concurrencia, otras
formas de tipos, globales TLS ni la identidad entre multiples sesiones. T08-004
y la tanda 8 permanecen abiertos.
