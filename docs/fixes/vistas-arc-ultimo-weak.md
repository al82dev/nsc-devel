# Último WeakArc durante destrucción con Core de vistas

La selección existente `tests/integration/shared_arc_public_last_weak.py`
coordina dos hilos: el destructor del último `Arc` espera mientras otro hilo
libera el último `WeakArc` explícito. Comprueba una destrucción por ronda y
un débil vacío al terminar. La ejecución previa descrita en
`shared-atomic-last-weak.md` usó otro Core; este control usa el SDK binario de
vistas. No se modificó la fuente del caso: la copia
`A:/nsc-vistas-evidencia/shared-arc-public-last-weak-cwd.py` solo adapta el
directorio de trabajo y la ruta de la copia del repositorio, como muestra `fc`.

Con `vistas-env.cmd`, se ejecutó el fixture con
`A:/nsc-vistas-evidencia/build/bin/nsc.exe` y
`A:/nsc-vistas-evidencia/sdk/NativeSharpCore.lib`, sin caché de artefactos.
La repetición `shared-arc-public-last-weak-vistas-hashed.log` terminó 0:
O0 y O2 devolvieron 42 tras 64 rondas cada uno. El SHA-256 del compilador
antes y después fue
`86e5ceae12e054389adcbe9f3140e7b811e5e4c7f3a98d99a3ebe3ba1c38fdbd`.
El Core y su metadata tenían hashes
`565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6` y
`8213d5eca49219a8322137d2bba05916b71df8a700c9f2bfd04a7089d4ed62d9`,
respectivamente. La ejecución anterior se conserva en
`shared-arc-public-last-weak-vistas.log`; no se midió su hash previo.
Todos los logs y temporales de esta selección están en A.

La prueba observa ese orden coordinado, no todas las intercalaciones ni la
liberación física final del bloque. No se usó ASan, no se probó descarga de
módulo y el resultado no certifica ausencia general de carreras o fugas.
