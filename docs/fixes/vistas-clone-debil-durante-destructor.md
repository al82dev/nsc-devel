# Clonación débil durante el último destructor

La prueba `tests/integration/shared_reentrant_weak_clone_vistas.py` comprueba
un caso distinto de leer el conteo o intentar `Upgrade()` reentrantemente. Un
débil explícito permanece vivo mientras el payload del último fuerte se
destruye. Dentro del destructor, en el mismo hilo, se clona ese débil: ambos
deben estar expirados y observar dos débiles explícitos. Al abandonar el ámbito
del clon debe volver a observarse uno; el programa también exige una sola
destrucción y que el débil original siga expirado después.

La observación se hace mediante un puntero `unsafe` temporal al débil local,
que vive durante todo el destructor. Esta forma evita almacenar `WeakRc` en un
campo estático y no transfiere el puntero a otro hilo.

Con `vistas-env.cmd`, el comando `python tests/integration/shared_reentrant_weak_clone_vistas.py
A:/nsc-vistas-evidencia/build/bin/nsc.exe A:/nsc-vistas-evidencia/sdk/NativeSharpCore.lib`
terminó 0: Rc/WeakRc y Arc/WeakArc compilaron y devolvieron 42 en O0/O2. El
log es `A:/nsc-vistas-evidencia/shared-reentrant-weak-clone.log`; fuentes,
comandos y hashes antes/después están en
`A:/nsc-vistas-evidencia/temp/shared-weak-clone-in-drop-htnjbpne/results.json`.
SHA-256 invariables: compilador
`9720edc91c6e9c4f0d6c62167f1e90802c8ace76da1fddec6ac3cdef795ed988`, Core
`565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6` y
metadata `8213d5eca49219a8322137d2bba05916b71df8a700c9f2bfd04a7089d4ed62d9`.
El fixture recibe sus artefactos por argumentos, usa cwd del repositorio y
temporales del entorno en A.

Esta evidencia cubre el orden descrito en un mismo hilo; no demuestra todas las
intercalaciones concurrentes, liberación física del bloque, excepciones dentro
del destructor ni descarga de módulos. El build contiene trabajo concurrente y
no certifica la rama completa.
