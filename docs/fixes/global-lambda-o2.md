# Lambda global sin captura en O0 y O2

La regresión del caso 69 compilaba y ejecutaba la lambda global sin captura en
O0. El ejecutor ahora comprueba también O2, sin cambiar el fixture.

El 11 de noviembre de 2026 se ejecutó el mismo script en una copia aislada,
con un fixture idéntico byte por byte al de esta rama. Los dos programas
terminaron con salida 0. El comando y el registro están en
`A:/nsc-agent3-evidencia/hybrid/global-lambda-directed-o0-o2.cmd` y `.log`.
Se utilizó el compilador autorizado del build aislado, con SHA-256
`2e637fcf042040f9c8a1f5dd4dcace333c67cd01e115a9ff55fa5eeadde2178c`,
runtime generado en A y `nslink.exe` por nombre. El compilador contenía otros
cambios locales; esta prueba no acredita una revisión limpia ni todas las
formas de lambda con captura.
