# T08: enum C unsigned int con int

El caso `EnumMixed` devolvía 7: al comparar el enum C `CUnsigned` de valor `0xffffffff` con el `int` -1, NSC ampliaba ambos a i64, el primero sin signo y el segundo con signo; Clang los compara como `unsigned int` de 32 bits. El mismo criterio equivocado hacía la suma en 64 bits antes de comparar con C.

Se reconoce exclusivamente la combinación de enum C importado con base `uint` e `int`, en cualquier orden. El análisis aritmético selecciona `uint`; HIR emplea ese tipo para comparaciones y aplica la conversión numérica C al operando `int`. No se alteran las conversiones entre tipos exclusivamente NativeSharp ni otras combinaciones. El IR resultante contiene comparación y suma `i32`, con extensión posterior cuando se asigna a `long`.

El fixture comparó igualdad y suma con funciones compiladas por Clang, en ambos órdenes, e incluye comparaciones de orden sin signo. Pasó O0–O3. También pasó el control existente de aritmética de enum C y bitwise de enum NativeSharp. La validación integral sigue pendiente por los fallos TLS y por la siguiente ejecución agrupada. NSLink no se modificó.
