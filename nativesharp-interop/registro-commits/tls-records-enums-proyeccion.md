# Proyección TLS de records y enums C

El control C pasó para struct, enum, sus punteros y un arreglo de struct;
NSC rechazó sus globales en la barrera TLS. Se permite que esos tipos lleguen
al mapeo existente, que valida identidad, origen, layout y representación;
el TLS C++ y las formas no mapeadas siguen rechazadas. La construcción y las
pruebas agrupadas, incluidas estático y DLL, están pendientes.
