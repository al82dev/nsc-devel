# T08-004: validacion de punteros y agregados C internos

La construccion incremental y el control C/NativeSharp pasaron O0–O3 para
int**, union, arreglos de record y enum, y arreglos const y volatile de int.
El intento de escribir en el arreglo const produjo el diagnostico esperado;
load acquire y store release volatile de 32 bits coincidieron en los IR de
Clang y NSC. Pasaron las regresiones de tipos internos, calificadores y enums C.

El control se agrega al gate de la tanda, aun pendiente de ejecucion completa.
No se acreditan otros tipos, formas de inicializacion, TLS ni todos los usos de
arreglos calificados. T08-004 permanece abierto.
