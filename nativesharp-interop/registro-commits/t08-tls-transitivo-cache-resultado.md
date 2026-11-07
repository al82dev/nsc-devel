# Resultado: caché y cambio de dependencia a TLS

`validar-tls-transitivo.cmd` terminó con código 0. El consumidor inicialmente
usó NSLink con la biblioteca C sin TLS; tras reconstruir el productor con TLS,
ambas construcciones usaron LLD, ejecutaron correctamente y conservaron el
directorio TLS. La segunda informó `artifact-cache-hits: 1` para objetos; no se
afirma un acierto de bundle. Salidas:
`A:/nsc-interop-build/tls-abi/transitive-test-19242-24286` y
`A:/nsc-interop-build/tls-abi/transitivo-cache-validacion.log`.
Los controles C de TLS de DLL y la tanda completa siguen abiertos.
