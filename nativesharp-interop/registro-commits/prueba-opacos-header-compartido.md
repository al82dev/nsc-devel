# Tipos C opacos con header compartido

Se prepara un consumidor que importa el mismo encabezado C que el productor
NativeSharp precompilado. Cruza punteros de struct y union en ambos sentidos
entre funciones C y métodos publicados y los asigna a globales C. La prueba
aún no se ha ejecutado; no acredita conciliación de identidades.

Con build `6b5448c0a735018f`, el productor y el consumidor sin header pasaron
en O0; el consumidor con el header compartido se detuvo antes del enlace con
`record observation name lookup failed: C/C++ type alias conflicts with a
native type 'Opaque'`. No se ejecutaron O1–O3 ni los controles posteriores.

La ruta de conciliación actual requiere `compareCRecordClosureEvidence`. La
captura retorna de inmediato para grupos `ForwardOnly` sin copiar procedencia;
la comparación considera insuficiente cualquier record incompleto y el codec
de procedencia vigente rechaza guardarla para records incompletos. Estas
condiciones explican por qué el mecanismo existente no acredita equivalencia de
ambas declaraciones adelantadas. Antes de habilitarla hay que comprobar que
Clang aporta procedencia directa, conservar compatibilidad de versiones y
probar encabezados distintos o modificados; igualdad de nombre o USR no basta.
