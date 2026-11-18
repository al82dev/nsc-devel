# ASan: doble liberacion con adaptador integrado

En Windows x64 se probo el objeto adaptador generado por el driver ASan del compilador aislado. El fixture `A:/nsc-fixes-auditoria-evidencia/asan-double-free-probe/probe.py` compilo dos programas IR con Clang de referencia y los ejecuto en procesos separados. La configuracion predeterminada elimino `ASAN_OPTIONS` del entorno; la segunda uso `ASAN_OPTIONS=quarantine_size_mb=0`. Los temporales y resultados se mantuvieron en A, sin cambiar PATH.

La liberacion unica devolvio 42 sin informe ASan en ambas configuraciones. La doble liberacion con opciones predeterminadas produjo `AddressSanitizer: double-free` y salida 1. Con cuarentena cero el adaptador rechazo la operacion con su diagnostico generico y salida 87. Esto reproduce la limitacion de diagnostico previamente documentada, sin demostrar un defecto nuevo.

Comandos, salidas, hashes y opciones: `A:/nsc-fixes-auditoria-evidencia/asan-double-free-20261112T053855842763Z/results.json`. Los hashes de entradas coinciden antes y despues; el objeto adaptador generado tiene SHA-256 `132ce63bc42c8b7a344be9b3b29feecb4d23090c0a5e8d0fa36816a193531cf5`. Clang advirtio que reemplazaba el triple del modulo.

El ensayo usa un objeto generado por el driver y fixtures IR enlazados por separado; no ejecuta un programa NativeSharp con doble liberacion ni certifica todos los tipos de asignacion o configuraciones ASan. El compilador corresponde a un arbol con cambios locales.
