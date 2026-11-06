# Dependencia C compartida por productores precompilados

Dos bibliotecas NativeSharp precompiladas que declararon el mismo `api.lib`
produjeron NSL1021 para `external_double` al enlazar su consumidor. Las
bibliotecas de los productores no contienen `api.lib`: la dependencia se
resuelve desde su metadata y `validateLibraryReference` la agregaba una vez
por cada productor sin consultar las rutas ya añadidas.

El ajuste evita volver a agregar la misma ruta ya suministrada por un productor
anterior o por el proyecto. Conserva el orden y las repeticiones dentro de cada
lista individual y mantiene la verificación de identidad de las dependencias.
NSLink no se modifica. Se restituye la dependencia en el segundo productor para
ejercitar el caso original; queda pendiente la construcción y validación.

## Resultado del 2026-11-06

Con el build incremental `66725a00ab433111`, el caso original con ambos
productores declarando `api.lib` compiló, enlazó y ejecutó en O0–O3; ambos
estados estáticos continuaron independientes. La matriz de metadata C existente
pasó y `nsc.cpp-interop.session` pasó 1/1. El resultado no demuestra todos los
grafos, órdenes ni repeticiones de bibliotecas; no se modificó NSLink.

Se amplía el control para que ambos productores invoquen la dependencia C y
para que el consumidor la declare y llame directamente. Pendiente de ejecución.

El fixture ampliado pasó O0–O3 con el build `66725a00ab433111`: ambos
productores llamaron a la función externa, el consumidor la llamó directamente
y los estados estáticos independientes conservaron los valores esperados.
El enlace informó tres bibliotecas: los dos productores y una dependencia C.
No se prueban todavía todos los órdenes ni grafos de bibliotecas.

Se prepara un control adicional: el consumidor utiliza también las funciones
inline con estado del mismo encabezado, y comprueba que su contador y variable
estática son independientes de los dos productores. Pendiente de validación.
