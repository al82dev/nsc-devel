# TLS homónima en DLL distintas

Dos productores precompilados utilizan `tls_exported`, pero cada uno la importa
de una DLL diferente: `api.dll` la inicia en 3 y `api2.dll` en 7. El consumidor
debe obtener ambos valores mediante sus respectivas plantillas.

En O0 ambos productores y el consumidor compilaron y enlazaron, pero el
ejecutable devolvió 1 porque no obtuvo los dos valores esperados. La inspección
de importaciones mostró `api.dll` y no `api2.dll`. El comportamiento incorrecto
es silencioso durante la compilación.

`tls-dll-distintas/validar.cmd dos` reproduce el caso y debe pasar cuando se
conserve el origen de cada variable. La conciliación actual compara símbolo,
USR, tipos y calificadores, pero no identifica la DLL propietaria. El cambio de
esa frontera y la representación de importaciones múltiples siguen pendientes;
no se modificó NSLink. Esta reproducción no se integra al gate mientras falla.
