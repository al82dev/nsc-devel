# Clon débil reentrante y destructor que lanza

`tests/integration/shared_reentrant_survivor_throw_vistas.py` combina dos
controles anteriores: clonar un débil dentro del destructor del último dueño y
propagar una excepción desde ese destructor. El destructor guarda el clon en un
segundo local exterior y luego llama a un inicializador estático que lanza.
Tras capturar la excepción, la prueba exige una destrucción, dos débiles
explícitos y expiración; libera el débil original y comprueba que el clon
sigue expirado, cuenta uno y no permite `Upgrade()`.

Dos punteros `unsafe` apuntan a locales declarados antes del dueño y vivos
durante el destructor; se utilizan en un único hilo. El comportamiento
corresponde a las cuatro familias públicas del Core binario de vistas.

Con `vistas-env.cmd` se ejecutó `python
tests/integration/shared_reentrant_survivor_throw_vistas.py
A:/nsc-vistas-evidencia/build/bin/nsc.exe A:/nsc-vistas-evidencia/sdk/NativeSharpCore.lib`.
Rc/Arc, payload clase/struct y O0/O2 sumaron ocho ejecuciones con resultado
42; el ejecutor terminó 0. Log
`A:/nsc-vistas-evidencia/shared-reentrant-survivor-throw.log`; fuentes,
comandos y hashes en
`A:/nsc-vistas-evidencia/temp/shared-weak-survivor-throw-qxnld2gn/results.json`.
Los SHA-256 antes y después coincidieron: nsc
`339957f565c52997e9278150bbf13f33700aac6562bf80b8e5eac270bc0e2688`, Core
`565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6`,
metadata `8213d5eca49219a8322137d2bba05916b71df8a700c9f2bfd04a7089d4ed62d9`.
Procesos con cwd del repositorio, temporales en A y `--no-artifact-cache`.

La evidencia no cubre concurrencia, liberación física del bloque, descarga de
módulos ni todos los recursos de un payload cuyo destructor falla. El build
tenía trabajo concurrente; esta prueba no certifica la rama completa.
