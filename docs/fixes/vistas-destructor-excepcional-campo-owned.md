# Campo owned cuando el destructor lanza

La evidencia anterior de `shared-destructor-weak-cleanup.md` no medía la
destrucción de campos del payload cuando su destructor se interrumpe. La
prueba `tests/integration/shared_throwing_destructor_fields_vistas.py` crea un
payload con un campo `owned Resource`; su destructor registra una entrada y
llama a un inicializador estático que lanza. El programa captura la excepción
y exige una sola entrada al destructor y una sola destrucción del recurso.

El control compara construcción directa con las familias públicas Rc y Arc
del Core binario; prueba payload de clase y estructura en O0/O2. Con
`vistas-env.cmd`, el comando `python
tests/integration/shared_throwing_destructor_fields_vistas.py
A:/nsc-vistas-evidencia/build/bin/nsc.exe A:/nsc-vistas-evidencia/sdk/NativeSharpCore.lib`
terminó 0 y las 12 ejecuciones devolvieron 42. El log está en
`A:/nsc-vistas-evidencia/shared-throwing-destructor-fields-portable.log` y los
comandos, fuentes y hashes en
`A:/nsc-vistas-evidencia/temp/shared-throw-drop-fields-6cx969tv/results.json`.
Los SHA-256 antes y después coincidieron: nsc
`6ed5d6264bf913325c858f81a395ff9bab827d7d49f40b10e9938db25ffed087`, Core
`565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6` y
metadata `8213d5eca49219a8322137d2bba05916b71df8a700c9f2bfd04a7089d4ed62d9`.
Los procesos usaron cwd del repositorio, temporales en A y `--no-artifact-cache`.

El resultado verifica este campo y esta ruta de excepción. No mide la
liberación física de memoria del recurso ni del bloque compartido, no cubre
múltiples campos, excepciones anidadas, concurrencia ni descarga de módulos.
El compilador contenía otros cambios locales; no se certifica toda la rama.
