# Reproducción de TLS tras dos bibliotecas NS

El consumidor solo referencia `Intermediate.lib`; su metadata referencia
`TlsProducer.lib`, que registra la biblioteca C TLS y el CRT. La emisión directa
sin elección explícita eligió NSLink y falló NSL1023. Registro:
`A:/nsc-interop-build/tls-abi/cadena-estatica-reproduccion.log`. La corrección
debe inspeccionar también las dependencias NS transitivas antes de la caché.
