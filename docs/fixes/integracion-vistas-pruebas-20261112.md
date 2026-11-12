# Integración dirigida de pruebas de punteros de vistas

Se integraron con `cherry-pick -x` los commits de la rama local
`development-fixes-limitaciones-vistas`: `8b60ff220` → `c7d4d9238`,
`8d54e7a07` → `8489d9fdb`, `6193042bf` → `b506af750` y `4f0a7070b` →
`4dc372ccf`. Solo agregan pruebas y sus documentos; no integran los cambios
de compilador que permanecen en esa rama ni certifican sus cambios sin registrar.

Los cuatro runners se ejecutaron desde `A:/nsc-devel-fixes` mediante
`A:/nsc-fixes-evidencia/integracion-vistas-prueba-20261112.cmd`, con `TEMP`,
`TMP`, `APPDATA` y `LOCALAPPDATA` en A, el runtime aislado y `nslink.exe` por
nombre. Se utilizó `A:/nsc-fixes-evidencia/build/bin/nsc.exe` SHA-256
`223655d766ecd7309b67b08593ed2619da19c348da4fce894d9b83a042680545`.
El Core de esta ejecución proviene de `lib/NativeSharp.Core/NativeSharp.Core.nsproj`
de la copia integradora y está en `A:/nsc-fixes-evidencia/integracion-vistas-sdk`:
la biblioteca tiene SHA-256 `565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6`
y su metadata `7d5921ca90d909c1e7fa98d092b9c0c28104dfe56f60f940e5deb89096203ccf`.
La generación se solicitó con la caché existente, pero el log registra
`compilation-bundle-cache: not-found` y procesamiento de los 71 archivos de
Core. Fue una reconstrucción del SDK, no una validación incremental de caché;
no debe repetirse como rutina. El usuario exige compilaciones incrementales.

| Prueba dirigida | Resultado |
|---|---|
| `shared_reentrant_upgrade_vistas.py` | Rc y Arc, O0/O2: 4/4 |
| `shared_reentrant_weak_clone_vistas.py` | Rc y Arc, O0/O2: 4/4 |
| `shared_throwing_nested_constructor_vistas.py` | Rc y Arc, clase y struct, O0/O2: 8/8 |
| `shared_reentrant_weak_survivor_vistas.py` | Rc y Arc, O0/O2: 4/4 |

Los logs `A:/nsc-fixes-evidencia/integracion-shared_*-20261112.log` conservan
salidas y la ruta de cada `results.json`; estos registran comandos, fuentes,
resultados y hashes de entrada. Los 20 casos compilaron y devolvieron 42. Son
controles específicos de ciclo de vida, no una prueba general de seguridad de
punteros ni de concurrencia. Se mantiene pendiente el caso distinto de una
fábrica que lanza con cuatro campos de familias compartidas, y la auditoría de
los commits funcionales de vistas y sus interacciones con otras ramas.
