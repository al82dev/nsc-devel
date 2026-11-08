# T08: recuperación de una muestra histórica NSMETA 1.45

Se conservan sin recodificar los bytes de
`A:/nsc-interop-build/tanda08-opaco-c/COpaqueProducer.nsmeta` en
`tests/fixtures/nsmeta/historical-1.45.nsmeta`. SHA-256:
`5b06c0f722319ef440ddd7b22e948051724422a61c32037e330cbeb12e2e0db1`.

El archivo declara productor `3.0.0+build.20261107T120447Z.420afcc5a0571d78`,
formato 1.45, módulo `COpaqueProducer` y destino `x86_64-pc-windows-msvc`.
La marca incorporada precede al commit `d801af165`, que introdujo el formato
1.46, fechado 2026-11-07 23:54:47 -0600. El commit exacto del build no está
demostrado; la fecha del archivo por sí sola no se usa como identidad.

El NSC de desarrollo build `5c1e6c7fa15d8a3e` ejecutó
`--verify-library-metadata` sobre el original con código 0: reconoció NSMETA
1.45, Object ABI 3 y huella ABI
`b969726161c5bfa5e3c2b5fd2534096eaf69acf968b928d4490076519182889f`.
La suite completa todavía no se ha repetido y falta una muestra histórica
independiente 1.46. El gate y la tanda permanecen abiertos.
