# Conteo débil atómico durante destrucción reentrante

Con un `WeakArc` explícito, el destructor del último `Arc` consulta
`WeakCount()` en el mismo hilo. El fuerte ya es cero pero el débil implícito
sigue retenido hasta terminar el destructor: el resultado anterior era 2,
aunque solo había un débil explícito. La reproducción pública desde fuentes
terminó con 1 en O0 (`public-tuple-borrow-atomic_reentrant_weak_count-Arc.log`).
Una prueba independiente del control atómico terminó con 3 en O0 y O2
(`atomic-reentrant-before.log`).

`SharedAtomicControl` usa la palabra 2 reservada del bloque para indicar la
existencia del débil implícito. Se inicializa antes de publicar el bloque;
después se lee y actualiza mediante las operaciones atómicas del SDK. La marca
se retira en el `finally` inmediatamente antes de liberar el débil implícito,
conservándolo durante el destructor. `WeakCount` resta esa marca de su lectura
del contador. Las dos lecturas son independientes: ante cambios concurrentes el
resultado sigue siendo una observación aproximada, como aprobó el usuario.

La regresión nueva comprueba el conteo antes, durante y después de la destrucción,
la liberación del débil explícito dentro del destructor y una excepción implícita
en el destructor. Pasó en O0 y O2 (`atomic-reentrant-after.log`). Las cuatro
pruebas dirigidas `nsc.sdk.shared-atomic-*`, incluidas contienda de upgrade y
liberación del último débil durante destrucción, pasaron 4/4; el control de
excepción en destructor pasó 1/1 (`atomic-reentrant-controls.log`). El caso
público de `WeakArc` pasó desde fuentes en O0 y O2
(`public-tuple-borrow-atomic_reentrant_weak_count.log`).

Estas pruebas no demuestran seguridad para todas las intercalaciones ni el
envío de la API pública entre hilos. El SDK binario y la retención del módulo
productor siguen pendientes.
