# Fábricas con excepción y SDK binario

El consumidor `tests/integration/shared_binary_public_pointers.py` ejecuta
`MakeRcThrowing` y `MakeArcThrowing` con una estructura cuyo constructor
inicializa un recurso `owned` y luego lanza. Contra
`A:/nsc-fixes-evidencia/generic-owner-sdk42/NativeSharpCore.lib` y su metadata,
con el compilador aislado build `cc5224c5c9250771`, los cuatro cruces Rc/Arc y
O0/O2 devuelven 42: la excepción llega al consumidor y el recurso se destruye
una vez. El mismo recorrido de prueba comprueba los demás controles indicados
en `shared-binary-public-pointers.log`.

Una variante adicional usa una clase `Bad` con constructor y campo propiedad
análogos. La configuración `NSC_SHARED_BINARY_THROWING_CLASS=1` conserva esta
reproducción. Con `A:/nsc-fixes-evidencia/shared-binary-throwing-class.cmd`,
Rc en O0 no llega a ejecutar: el compilador rechaza la generación con
`shared throwing construction cannot safely clean this payload shape`.
El resultado y comando están en `shared-binary-throwing-class.log`. La prueba
se detiene en ese primer fallo: no acredita todavía O2 ni Arc para la clase.

El consumidor desde fuentes cubre un patrón equivalente en los controles de
`shared-public-pointers`. La ruta binaria añade una base precompilada a las
clases del consumidor y la comprobación actual rechaza bases precompiladas;
esto es una hipótesis de causa, no una demostración de que toda base importada
sea trivial. Se necesita acreditar sus campos, destructores y cadena de bases
antes de relajar ese rechazo. El defecto para clase sigue pendiente y no se
considera una regresión aprobada ni una garantía general de limpieza.
