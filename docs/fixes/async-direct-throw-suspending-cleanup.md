# Publicación del error tras un finally suspendible

En la revisión `9e42cefac`, la reproducción original
`generator_direct_cleanup_suspends` de `async_poll_call_exception.py` devolvía
3 en O0 y O2. Una copia diagnóstica independiente mostró que la excepción se
capturaba, pero el efecto posterior al `await` del finally todavía no había
ocurrido. El MIR guardaba el estado de error antes de ejecutar ese finally.

`rewriteAsyncThrow` difiere esa publicación para una salida por excepción con
cleanup; `lowerStructuredExit` publica el estado de error al completar la
limpieza. Se conserva la publicación previa para las demás formas de salida.
La prueba dirigida `async_direct_throw_suspending_cleanup.py` pasó en O0 y O2
para el ejemplo original de generador, un async ordinario con finally
suspendible y el control de generador sin cleanup. También pasaron los 14
controles de `async_poll_call_cleanup_nonsuspending.py` en ambos niveles. La
evidencia se conserva en `A:\nsc-fixes-evidencia\async-direct-throw-cleanup-regression-20261112.log`
y `A:\nsc-fixes-evidencia\async-poll-cleanup-regression-after-20261112.log`.

Siguen fallando la propagación de error desde un future hijo con finally
suspendible y las llamadas lanzadoras bajo dicho finally; la presente
corrección no acredita esos caminos ni excepciones externas.
