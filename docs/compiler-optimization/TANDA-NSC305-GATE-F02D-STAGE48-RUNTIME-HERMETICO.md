# Tanda NSC305 GATE-F02D — runtime hermético de Stage 48.2

## Alcance

Esta hoja corrige exclusivamente el contrato de ejecución del CTest
`nsc.stage48_2.library-metadata`. No cambia el compilador, el linker, NSMETA, ABI, formatos,
fixtures ni expectativas funcionales. GATE-F02A y GATE-F02B conservan por separado los fixtures
históricos 1.38 y 1.39; GATE-F02E conserva la mejora independiente de observabilidad y evidencia.

## Causa demostrada

Stage 48.2 ejecuta productos reales `--emit-project-oop-lib` y `--emit-project-oop-dll`. Esas
rutas entregan a `WindowsLibraryBuilder` el resultado de `configuredWindowsRuntimeLibrary()` y,
por tanto, requieren la biblioteca runtime producida por el mismo build.

El target `nsc` ya genera en CMake `${NSC_WINDOWS_RUNTIME_LIBRARY}`, pero el `add_test` de Stage
48.2 no publicaba esa ruta en su entorno. Para el ejecutable situado bajo `tests`, la búsqueda
relativa apunta a `lib/nativesharp/runtime`, no a `generated`; el fallback compilado procede de
`CMAKE_INSTALL_PREFIX` y no constituye una entrada hermética del build en A:.

La evidencia contrafactual ya existente discrimina esta causa:

- D04 ejecutó el mismo agregado con `NSC_WINDOWS_RUNTIME_LIBRARY` apuntando al runtime generado
  en A: y obtuvo 393/398 checks.
- F02C ejecutó con PATH Machine+User y `nslink.exe` correctamente resuelto por nombre, pero sin
  la variable runtime; completó 308/382 y expuso 70 fallos de productos o verificaciones
  dependientes, además de los cuatro checks F02A/F02B.
- Las acciones de metadata que no construyen LIB/DLL continuaron pasando. Los fallos se agrupan
  en las rutas que materializan productos y sus consumidores.

## Corrección

El CTest recibe una única variable adicional:

`NSC_WINDOWS_RUNTIME_LIBRARY=${NSC_WINDOWS_RUNTIME_LIBRARY}`

La derecha es el artefacto `generated/NativeSharp.Runtime.Win32.lib` del mismo árbol de build.
No se agrega `NSC_LINKER` al test, no se fija una ruta absoluta al ejecutable y no se modifica
`PATH`: NSLink debe seguir resolviéndose exclusivamente como `nslink.exe` desde el PATH nativo del
proceso.

## Evidencia focal

La validación se ejecutó con `TEMP` y `TMP` en A:, reconstruyendo el PATH del proceso sólo desde
Machine+User. `where.exe nslink.exe` resolvió exactamente
`A:\nslink-devel\bin\release\nslink.exe`; el gate fijó `NSC_LINKER=nslink.exe`, nunca una ruta al
ejecutable.

- Reconfiguración incremental y builds de `nsc` y
  `nsc_stage48_2_library_metadata_tests`: PASS.
- El post-build regeneró
  `A:\nsc-devel\build-test-infrastructure\generated\NativeSharp.Runtime.Win32.lib`.
- CTest mostró explícitamente en su entorno
  `NSC_WINDOWS_RUNTIME_LIBRARY=A:/nsc-devel/build-test-infrastructure/generated/NativeSharp.Runtime.Win32.lib`.
- Stage 48.2 completó 398 checks: 394 PASS y cuatro FAIL.
- Los 70 fallos de productos/cascadas desaparecieron por completo.
- Los cuatro fallos residuales son exactamente lectura y compatibilidad de
  `historical-1.38.nsmeta` y `historical-1.39.nsmeta`, asignados a F02A/F02B.
- No apareció ningún diagnóstico causal nuevo.
- Logs persistentes: `A:\temp\gate-f02d-where-nslink.log`,
  `A:\temp\gate-f02d-build.log` y `A:\temp\gate-f02d-ctest-stage48.log`.

El exit global de CTest permanece rojo exclusivamente por F02A/F02B. F02D queda causalmente
cerrado: el agregado recuperó su comportamiento previo, el runtime provino del mismo build en A:
y NSLink continuó resolviéndose por nombre.
