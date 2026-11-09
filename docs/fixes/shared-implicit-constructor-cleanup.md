# Limpieza de campos ante una excepción implícita

Un constructor de instancia sin `throws` puede llamar a un método de otra clase
cuyo inicializador estático lanza. En las fábricas compartidas, una reproducción
con un campo `owned Resource` ya inicializado capturó esa excepción, pero no
destruyó el recurso: `Rc` y `Arc`, con clase y struct, devolvieron 13 en O0 y O2
en vez de 42. El código 13 identifica específicamente el destructor ausente;
la excepción fue capturada y no se llamó el destructor del payload incompleto.
Fuente y resultados anteriores: `A:/nsc-fixes-evidencia/static-factory-leak-probe/`,
archivos `*-late2.log`.

El backend solo generaba el cleanup de placement cuando el constructor de
instancia declaraba `throws`. Para `SharedControlStorage.Construct` y
`ConstructValue`, ahora también lo genera ante una salida excepcional implícita
si el almacenamiento se inicializa en cero y los campos y bases satisfacen las
comprobaciones de limpieza parcial ya existentes. El cleanup destruye campos
propietarios en orden inverso sin llamar el destructor del payload incompleto;
la fábrica libera después la reserva mediante la corrección anterior.

Las cuatro reproducciones devolvieron 42 en O0 y O2 (`*-late3.log`). La nueva
prueba `nsc.sdk.shared-implicit-constructor-cleanup` comprueba para las dos
familias internas y ambas categorías un fallo anterior al campo, uno posterior
y una construcción y destrucción normales. El build fue incremental y esa
prueba, `shared-static-init-cleanup` y `shared-throwing-factory` pasaron 3/3
(`A:/nsc-fixes-evidencia/implicit-constructor-cleanup-ctest.log`).

No se amplió la aceptación de bases no triviales ni de campos de valor que
requieren drop: esos casos mantienen sus limitaciones y requieren seguimiento
de subobjetos construidos. La prueba usa fuentes de los controles internos;
las reproducciones con `Rc` y `Arc` públicos también usan fuentes, no prueban
el SDK binario ni transferencia entre hilos.
