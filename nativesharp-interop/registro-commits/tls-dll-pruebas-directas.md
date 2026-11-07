# Pruebas de acceso generado a TLS de DLL

`validar-nsc-directo.cmd` conserva el fixture original de NativeSharp con
lectura y escritura del global TLS y usa el control PE en C para preparar la
DLL y comprobar aislamiento. Ejecuta el programa en O0–O3 mediante NSLink.
El fixture original del consumidor C directo permanece para registrar su fallo.

La barrera negativa se adapta para exigir un diagnóstico cuando el mismo
global aparece con orígenes estático y DLL ambiguos. Ambos controles esperan
la construcción y ejecución agrupadas; no acreditan todavía la tanda completa.
