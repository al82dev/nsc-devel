# Validación agrupada de la tanda 18

El compilador incluye los cambios de la tanda hasta `470b37b7c`:

- el cambio de producción, [constructores que lanzan](registro-commits/t18-constructores.md);
- las pruebas integrales: [zlib](registro-commits/t18-zlib.md), [ICU](registro-commits/t18-icu.md),
  [STL](registro-commits/t18-stl.md) y [paquete](registro-commits/t18-paquete.md).

Base de la tanda: `2dcb699d5`.

## Interoperabilidad con enlace estático (T18-015, 2026-11-15)

`lote-t18.cmd t18g` ejecuta, con cinco procesos, todos los guiones `validar*.py` de `pruebas-tanda-18-*` a
`pruebas-tanda-09-*` y `lote-ct2.cmd`. Registros: `logs/regresion-t18-t18g.log` a `logs/regresion-t09-t18g.log` y
`logs/lote-ct2.log`.

- Tanda 18: 4 guiones, todos pasan. `pruebas-tanda-18-paquete` se agregó después de esta corrida y pasa aparte
  con el mismo compilador (`A:/temp/nsc-t18-paquete-_0auatp4`).
- Tanda 17: 3 guiones, todos pasan.
- Tanda 16: 4 guiones, todos pasan.
- Tanda 15: 3 guiones, todos pasan.
- Tanda 14: 4 guiones, todos pasan.
- Tanda 13: 6 guiones, todos pasan.
- Tanda 12: 3 guiones, todos pasan.
- Tanda 11: 5 guiones, todos pasan.
- Tanda 10: 28 guiones, todos pasan.
- Tanda 9: de 1099 guiones pasan 1094, se omiten 2 y fallan 3. Es la línea base de las tandas anteriores:
  - los dos de Clang `cpp-variadica-preserve-none`;
  - `cpp-tamano-objeto/validar-indice-expresion` en la corrida paralela.

  Los tres de `lista-tamano-3.txt` pasan con un proceso (`logs/regresion-t09-t18g-tamano.log`).
- `lote-ct2.cmd`:
  - pasan las nueve CTests y `nsc_cpp_c_identity_tests`;
  - `ninja -k 0` no tiene errores de compilación y solo falla al enlazar pruebas, por los tres símbolos ya conocidos.

## Interoperabilidad con DLL (T18-016)

`lote-dll.cmd t18d` (tandas 18 a 10) y `dll-t09.cmd t18d` ejecutan con `--dll` cada guion que lo acepta: 61
guiones. `regresion-dll.py` omite los que no aceptan `--dll`. Registros: `logs/regresion-dll-t18-t18d.log` a
`logs/regresion-dll-t09-t18d.log`.

| Tanda | Guiones | Resultado |
|---|---|---|
| 18 | 4 | todos pasan |
| 17 | 3 | 2 pasan; `cache` falla en la corrida paralela |
| 16 | 4 | 3 pasan; `enlace` falla en la corrida paralela |
| 15 | 3 | 2 pasan; `callbacks` falla en la corrida paralela |
| 14 | 4 | 2 pasan; `configuracion` y `registros` fallan en la corrida paralela |
| 13 | 6 | 4 pasan; `constexpr` y `plantillas-ns` fallan en la corrida paralela |
| 12 | 3 | 2 pasan; `herencia` falla en la corrida paralela |
| 11 | 5 | todos pasan |
| 10 | 11 | 9 pasan; `inline` y `namespaces` fallan en la corrida paralela |
| 9 | 18 | todos pasan (tres procesos, sin carga ajena) |

En las 10 fallas, el control de Clang (`/nodefaultlib`) es el que devuelve 0, tras unos 30 s. NativeSharp no
participa. Esa corrida compartía la máquina con el CTest completo, con ocho procesos. Repetidos con un proceso
(`repetir-dll.cmd t18d-rep`, `logs/regresion-dll-t18d-rep.log`), pasan los 10. Resultado: 61 de 61.

## Pruebas de NSC ajenas a interoperabilidad (T18-014)

`ctest-completo.cmd t18` ejecuta las 704 pruebas de `build-tanda09`, con ocho procesos, después de
`ninja -k 0`. Registro: `logs/ctest-completo-t18.log`. Pasan 515 y fallan 189. Repetidas con un proceso
(`ctest-repetir.cmd t18-rep`), fallan las mismas 189 (`logs/ctest-completo-t18-rep.log`).

`ctest-base.py` vuelve a ejecutar con el compilador anterior a la interoperabilidad
(`build-clang/bin/nsc.exe`, build hash `fed0f138b743c7d7`) las pruebas cuyo comando nombra el compilador de
`build-tanda09`. Registro: `logs/ctest-base-t18.log`.

| Causa | Pruebas | Atribuible a la interoperabilidad |
|---|---|---|
| Ejecutable de prueba que no se construyó (`Not Run`); `ninja -k 0` no enlaza por los tres símbolos conocidos | 83 | no |
| Biblioteca `lib/NativeSharp.Core.IO/NativeSharpCore.lib` que no existe en este árbol | 27 | no |
| `nsc.selfhost.*`: scripts de CMake con requisitos del selfhost que este árbol no prepara | 25 | no |
| `nsc.stage3x/4x.object` y `stage35-recovery.mir-dump`: buscan `project OOP object emission: ok`, que la salida concisa (`78d3e25ca`, 2026-10-24, anterior a la interoperabilidad) reescribe como `Build succeeded.`; con el compilador anterior pasan | 12 | no |
| `nsc.debug.*` y `ns3.t36/t37`: fallan igual con el compilador anterior (`Module stream not present`) | 4 | no |
| Entorno: fixtures de sesión en `A:/temp/t09-session-root`; ubicación de NSLink (`stage46-1`) | 2 | no |
| Pruebas unitarias con el frontend compilado dentro, sin una construcción anterior con qué compararlas | 36 | no demostrado |

Las 36 unitarias quedan en la tarea T19-027: construir el árbol en el commit anterior a la interoperabilidad y
comparar. Las nueve CTests de interoperabilidad de `lote-ct2.cmd` y `nsc_cpp_c_identity_tests` pasan.

## Estado de la tanda 18

T18-001 a T18-022 tienen su evidencia. T18-023 queda abierto: el inventario tiene pendientes y hay
combinaciones sin fixture. La sección 19 de TRACKING tiene una tarea específica para cada uno
([pendientes](PENDIENTES-INTEROP-C-CPP.md); [informe](INFORME-FINAL-INTEROP-C-CPP.md)). No se modificó Clang ni
NSLink.
