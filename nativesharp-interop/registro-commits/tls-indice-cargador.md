# Índice TLS al cargar las DLL de escalares

Se añadió `pruebas-tanda-08-tls-c/directo/probe_tls_loader.py` para copiar la
DLL generada al directorio temporal `A:/nsc-interop-build/tls-abi`, poner
`0xffffffff` en la ubicación `AddressOfIndex` de la copia y cargarla desde
un proceso Python con `ctypes.WinDLL`. Después se lee esa ubicación de la
imagen cargada. No se altera la DLL original ni las expectativas del gate.

Con las DLL generadas por los scripts existentes se observó:

| Variante del productor | Resultado del script | Índice después de cargar |
| --- | --- | --- |
| `validar-nsc-escalares.cmd`, `/noentry` y sin importaciones | Control C pasó; NS O0 falló | `0xffffffff` |
| `validar-nsc-escalares-con-entry.cmd`, `DllMain` y sin importaciones | Control C pasó; NS O0 falló | `0xffffffff` |
| `validar-nsc-escalares-con-import.cmd`, con importación Win32 | Control C y NS O0–O3 pasaron | `0x7` |

En este experimento el valor centinela permaneció en las dos primeras
variantes y cambió en la tercera. Esto explica por qué no basta con suponer
que un índice cero en la DLL defectuosa corresponde a un bloque TLS válido;
no demuestra el mecanismo interno del cargador ni que todas las DLL sin
importaciones tengan el mismo comportamiento. La prueba con centinela usa
carga dinámica en un proceso Python; los controles originales conservan su
propia forma de carga.

Para reproducir cada fila, ejecutar primero el script indicado en la tabla y
después `python nativesharp-interop\pruebas-tanda-08-tls-c\directo\probe_tls_loader.py`.
El script toma la última `api.dll` producida en el directorio `scalars`; es
necesario ejecutar la pareja de comandos para cada variante, porque las tres
escriben la misma ruta. El valor numérico asignado puede variar entre procesos.

El control C de acceso directo a un TLS de otra DLL sigue sin enlazar con
Clang: el símbolo TLS requerido no coincide con el de la biblioteca de
importación. Repetir el experimento con `dllimport` en
`A:/nsc-interop-build/tls-abi/ProbeDllimport.c` produjo el diagnóstico de
Clang 23: no se permite combinar `dllimport` y `thread`. Esta limitación ya
estaba registrada en `tls-c-dll-directo.md` y no supone una corrección de
NSC. Los fallos originales y la validación de la tanda 8 permanecen abiertos.
