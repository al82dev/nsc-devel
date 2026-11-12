# T09-003: record C++ trivial en una funcion ordinaria Win64

La muestra declara una funcion C++ libre ordinaria que recibe y devuelve un
record trivial de dos enteros por valor y un alias de puntero compatible. El
control de Clang 23.1.0 paso en estatica y DLL O0-O3
(A:/temp/nsc-t09-cpp-record-cdecl-qqu698sz). El IR de referencia usa `i64`
para argumento y retorno y el simbolo C++ decorado de la convencion ordinaria
(A:/temp/t09-cpp-record-cdecl-oracle.ll). Antes del cambio NSC no proyectaba
las funciones (A:/temp/nsc-t09-cpp-record-cdecl-vasuh72s).

La proyeccion directa y de punteros acepta ahora `cdecl` para el mismo record
C++ con identidad, layout y operaciones triviales verificadas; el filtro de
otros tipos C++ y las reglas C se conservan. Pasaron ocho comparaciones entre
C++ y NativeSharp, con comprobacion del simbolo, la llamada indirecta, el
resultado, la identidad del puntero y el objeto original
(A:/temp/nsc-t09-cpp-record-cdecl-2b90rgn1). Pasaron 16 consumidores de
plantilla precompilada, con y sin encabezado, estatica y DLL O0-O3
(A:/temp/nsc-t09-cpp-record-cdecl-precompilado-48_d2jzp).

Diez usos integrados con copia, construccion, destruccion o asignacion no
admitidas, o con campos volatiles, produjeron el rechazo esperado
(A:/temp/nsc-t09-cpp-record-cdecl-rechazos-d7ylwqkk). Pasaron tambien 16
consumidores precompilados de records C y 16 de la muestra C++ `preserve_most`
(A:/temp/t09-cpp-cdecl-final-*.log). Los rechazos verifican el uso integrado,
no cada paso interno de proyeccion por separado.

El resultado cubre solo la muestra de ocho bytes para Win64; no demuestra
todos los layouts, tipos C++ o metodos. T09-003 y T11 siguen abiertos. Las
pruebas reproducibles estan en
[pruebas-tanda-09-cpp-record-cdecl](pruebas-tanda-09-cpp-record-cdecl/).
