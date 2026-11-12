# Clon débil que sobrevive al destructor

`tests/integration/shared_reentrant_weak_survivor_vistas.py` comprueba la vida
de un débil clonado dentro del destructor del último fuerte. El destructor
clona el débil original y guarda el clon en un segundo local exterior. Tras
retornar del destructor se libera el original; el clon debe seguir expirado,
observar un débil explícito e impedir `Upgrade()`. Antes de salir del
destructor se verifican dos débiles y, después, una sola destrucción.

La prueba usa dos punteros `unsafe` a los locales `weak` y `survivor`, ambos
creados antes del dueño y vivos mientras corre su destructor. Solo se usan en
ese mismo hilo. Este caso complementa el clon temporal que se destruye antes de
terminar el destructor, documentado en
`vistas-clone-debil-durante-destructor.md`.

Con `vistas-env.cmd`, se ejecutó `python
tests/integration/shared_reentrant_weak_survivor_vistas.py
A:/nsc-vistas-evidencia/build/bin/nsc.exe A:/nsc-vistas-evidencia/sdk/NativeSharpCore.lib`.
Rc/WeakRc y Arc/WeakArc compilaron y devolvieron 42 en O0 y O2; el ejecutor
terminó 0. Log `A:/nsc-vistas-evidencia/shared-reentrant-weak-survivor.log` y
fuentes, comandos y hashes en
`A:/nsc-vistas-evidencia/temp/shared-weak-survivor-in-drop-id0lg1yr/results.json`.
Los SHA-256 antes y después coincidieron: nsc
`9b8fd85b5326cb235214ef69d39beb2ea7657deb68387becfe505e7dadc21ae2`, Core
`565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6` y
metadata `8213d5eca49219a8322137d2bba05916b71df8a700c9f2bfd04a7089d4ed62d9`.
Los procesos usaron cwd del repositorio, temporales en A y `--no-artifact-cache`.

No se probó descarga de módulos, liberación física del bloque, excepción en el
destructor ni concurrencia. La revisión estaba acompañada de otros cambios de
trabajo y este control no certifica toda la rama.
