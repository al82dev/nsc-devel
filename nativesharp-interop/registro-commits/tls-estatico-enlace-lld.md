# Prueba de biblioteca C con TLS estático mediante LLD

El fixture conserva la prueba de aislamiento entre hilos y selecciona LLD
explícitamente con el CRT estático para resolver el directorio TLS de PE sin
cambiar NSLink. Se amplía de O0 a O0–O3. Esta selección no demuestra soporte
TLS en NSLink ni resuelve el acceso directo TLS desde una DLL.
