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
se detuvo en ese primer fallo. Una repetición posterior seleccionó cada
combinación por separado: `Rc` y `Arc`, O0 y O2, los cuatro rechazan con el
mismo diagnóstico usando el compilador restaurado build `f4776103f991ed5f`.
Los registros son `shared-binary-throwing-class-{Rc,Arc}-{O0,O2}.log` en
`A:/nsc-fixes-evidencia`; en los cuatro casos no se ejecuta el consumidor.

El consumidor desde fuentes cubre un patrón equivalente en los controles de
`shared-public-pointers`. La ruta binaria añade una base precompilada a las
clases del consumidor y la comprobación actual rechaza bases precompiladas;
la rama causal se confirmó con el diagnóstico indicado abajo. No demuestra que
toda base importada sea trivial. Se necesita acreditar sus campos, destructores
y cadena de bases antes de relajar ese rechazo. El defecto sigue pendiente y no se
considera una regresión aprobada ni una garantía general de limpieza.

Un diagnóstico temporal insertado en la rama que rechaza una base precompilada
identificó específicamente `NativeSharp.Core.Object` para esta reproducción.
El resultado se conservó en `shared-binary-throwing-class-base-diagnostic.log`;
el diagnóstico temporal se retiró del código fuente después de capturar la
evidencia. La confirmación de esta rama no demuestra que toda base importada
sea trivial ni que sea seguro ignorar destructores privados.

La inspección del productor `LibraryMetadataConstruction.cpp`, función
`collectMetadataMemberOrdinals`, muestra que la lista ordinaria de miembros
incluye miembros públicos o protegidos y excepciones específicas para
plantillas. Por tanto, la ausencia de un destructor privado en esa lista no
demuestra que no exista. Una posible solución es publicar un contrato explícito
de limpieza trivial, con formato versionado y controles de compatibilidad;
todavía no está implementada ni se ha probado su alcance.
