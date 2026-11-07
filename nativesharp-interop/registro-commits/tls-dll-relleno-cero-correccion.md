# Límite de acceso a TLS con relleno cero

El helper `cppDllTlsAddressHelper` calculaba el límite exclusivamente desde
`EndAddressOfRawData`. Se amplía ese límite con el `SizeOfZeroFill` de la
misma entrada TLS PE, conservando la comprobación inferior y las demás
validaciones. Así el global exportado dentro de la zona inicializada a cero
se calcula con el mismo desplazamiento relativo al inicio de la plantilla.

La reproducción está en `tls-dll-relleno-cero-prueba.md`. Con NSC build
`b151b46f0be04227`, `validar-relleno-cero.cmd` pasó el control C, NS O0–O3
con la DLL original y NS O0–O3 con relleno cero. Pasaron también
`validar-nsc-directo.cmd`, `pointer/validar.cmd` y `array/validar.cmd` para
sus matrices de optimización. La prueba nueva se incorporó al validador de la
tanda, sin retirar los controles existentes. No se ha cerrado la tanda 8.
No cambia NSLink ni altera los fixtures previamente fallidos.
