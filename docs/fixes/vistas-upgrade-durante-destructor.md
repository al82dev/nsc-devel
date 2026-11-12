# Upgrade reentrante durante el último destructor

La prueba `tests/integration/shared_reentrant_upgrade_vistas.py` cubre una
observación distinta del conteo débil reentrante y de intentar `Upgrade()`
después de terminar el destructor. Un débil explícito permanece vivo mientras
se destruye el payload del último fuerte; dentro del destructor, en el mismo
hilo, se intenta `Upgrade()` y se comprueba que produce un fuerte vacío, que
el débil está expirado y que su conteo es uno. Después se verifica una única
destrucción y que el débil sigue expirado. Un puntero temporal `unsafe` permite
al destructor observar el débil local sin almacenar `WeakRc` en un campo
estático; el local vive durante toda la observación.

Con `vistas-env.cmd`, el comando `python tests/integration/shared_reentrant_upgrade_vistas.py
A:/nsc-vistas-evidencia/build/bin/nsc.exe A:/nsc-vistas-evidencia/sdk/NativeSharpCore.lib`
terminó 0: Rc/WeakRc y Arc/WeakArc compilaron y retornaron 42 en O0/O2. Su log
es `A:/nsc-vistas-evidencia/shared-reentrant-upgrade-portable.log`; comandos,
fuentes y hashes anteriores y posteriores están en
`A:/nsc-vistas-evidencia/temp/shared-upgrade-in-drop-nmhsa5du/results.json`.
El SHA-256 de nsc fue
`8ada2fb71ccdda03a390869a2b25feb461c36721a610d10beae8225890790543`; Core y
metadata conservaron sus hashes `565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6`
y `8213d5eca49219a8322137d2bba05916b71df8a700c9f2bfd04a7089d4ed62d9`.
La prueba recibe nsc y Core por argumentos, ejecuta los procesos desde el
repositorio y usa los temporales del entorno en A. El primer ensayo se conserva
en `A:/nsc-vistas-evidencia/shared-upgrade-during-destructor.log`.

La prueba cubre el orden reentrante indicado y un débil explícito, no
intercalaciones de hilos, excepciones del destructor, liberación física del
bloque ni descarga de módulos. El compilador contenía otros cambios de trabajo;
la evidencia no certifica la rama completa.
