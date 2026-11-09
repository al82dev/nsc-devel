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
la implementación posterior se describe a continuación.

## Contrato NSMETA 1.43

El productor publica las identidades de clases raíz que no tienen campos de
instancia ni destructor, después de examinar todos los miembros, incluidos los
privados. El lector valida que cada identidad pertenezca a una clase raíz sin
campos y que no esté duplicada; la mezcla conserva y deduplica el catálogo.
El consumidor conserva la información de la metadata completa aun cuando la
superficie importada esté podada. El backend consulta esta garantía únicamente
para aceptar una base precompilada durante la limpieza de la construcción.
Una biblioteca anterior carece de la garantía y mantiene el rechazo anterior;
no se presume que una clase homónima o una raíz con destructor sea segura.

Con el compilador aislado build `c8f964a528e0342c` se regeneró Core en
`A:/nsc-fixes-evidencia/generic-owner-sdk43/`. El consumidor binario ejecutó
las fábricas con clases y estructuras, Rc y Arc, O0 y O2, con salida 42:
captura las excepciones y destruye los recursos previstos. La misma selección
comprobó propietarios, expiración y rechazo de la mutación ensayada de una
vista. Registro `A:/nsc-fixes-evidencia/shared-binary-sdk43-check.log`.

La prueba `nsc.ns3.t12.exported-generic-ownership` pasó 25 comprobaciones,
incluidas publicación, clases con destructor o campos excluidas, rechazo de
contrato incompatible con sus campos, mezcla y rechazo de escritura como
NSMETA 1.42. `nsc.library-metadata-merger` pasó. Los controles dirigidos desde
fuentes de punteros pasaron. El Core anterior de la campaña sigue rechazando
el caso de clase en Rc/O0, como se esperaba al no contener la garantía.

La prueba adicional `nsc.library-metadata-version-tolerance` falló 1 de 16
comprobaciones porque su fixture histórico tiene ABI de objetos 2 mientras el
compilador exige ABI 3. Se repitió sin modificar el fixture en la base
`152654e5a0e2d84d4c2f245ea7590488f0497430`: allí falla la misma comprobación con
el mismo diagnóstico. Registros `metadata-version-check.log` y
`metadata-version-baseline-check.log`, en `A:/nsc-fixes-evidencia/`. La prueba
no se considera aprobada; el resultado no demuestra compatibilidad completa.

Este contrato describe solo raíces sin campos ni destructor; no resuelve
destrucción de bases no triviales, cambios de ABI, DLL ni dependencias binarias
externas. `src/Interop/LibraryMetadata*.cpp` y su cabecera son metadata de
NativeSharp compartida con la rama de interoperabilidad y deben revisarse en la
integración posterior; no se modificaron fuentes de interoperabilidad C/C++.
