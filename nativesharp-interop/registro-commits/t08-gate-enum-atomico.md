# Tanda 8: incorporación de pruebas de enum atómico

La batería agrupada incluye los controles de enum atómico global, TLS y
aislamiento entre hilos. Los dos primeros ejecutan O0–O3 y comparan el IR;
el tercero comprueba el aislamiento con dos hilos. Los wrappers fijan el modo
sin pasar argumentos a la lista de scripts del gate.

Los tres accesos pasaron por separado con el build actual: evidencias en
`A:\nsc-interop-build\tanda08-atomic-enum-global-30509-17005`,
`A:\nsc-interop-build\tanda08-atomic-enum-tls-30506-6257` y
`A:\nsc-interop-build\tanda08-atomic-enum-hilos-30503-28276`.

La incorporación no implica que la batería completa se haya ejecutado ni
que la tanda esté cerrada. Las pruebas deben volver a pasar como parte de la
validación agrupada, después de integrar y resolver los demás frentes.
