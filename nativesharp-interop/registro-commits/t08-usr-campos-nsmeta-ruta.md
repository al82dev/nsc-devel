# Tanda 8: comprobar USR de campos a través de NSMETA

La prueba de sesión amplía el contraste de campos C: publica el catálogo originado en Clang en un archivo NSMETA, lo lee y autentica mediante el codec de NSC, reconstruye los campos y compara sus USR con la sesión viva. No cambia el formato ni el código de producción.

La construcción incremental de `nsc_cpp_session_tests` terminó con código 0 y 32 advertencias de inicializadores; la suite terminó con código 0 en A:. El archivo de prueba se escribió en `A:/nsc-interop-build/cpp-field-usr-roundtrip.nsmeta` y se leyó con autenticación antes de reconstruir los campos.

Esta comprobación no acredita el flujo completo de un productor y consumidor independiente ni la identidad de funciones, globales o aliases; T08-002 y los fallos TLS siguen abiertos.
