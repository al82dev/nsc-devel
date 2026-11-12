# Comprobación unitaria de la pasada ASan

En `A:/nsc-limitaciones-asan` se construyó el objetivo dirigido
`nsc_item389_native_sanitizer_tests` en el build aislado
`A:/nsc-asan-sdk-evidencia/build`. El comando invocado desde la raíz fue
`call ..\nsc-asan-auditoria2-evidencia\build-item389.cmd`; el script fija
`TEMP`, `TMP`, `APPDATA` y `LOCALAPPDATA` en
`A:/nsc-asan-auditoria2-evidencia` y ejecuta:

```text
cmake --build A:/nsc-asan-sdk-evidencia/build --target nsc_item389_native_sanitizer_tests
```

El build terminó con código 0. Regeneró la identidad de build y recompiló
`src/Basic/Version.cpp` antes de enlazar el ejecutable de prueba, sin editar
fuentes del repositorio. El compilador `build/bin/nsc.exe` mantuvo SHA-256
`49b8952fb6e0676b5ab35ca09b19df888e64a332ec3fe88cbcc04d81a868a47d`.
El coordinador confirmó que no tenía otro build activo en ese directorio;
`tasklist` devolvió `The user name or password is incorrect`, por lo que no
se acreditó la ausencia de procesos ajenos.

El comando `call ..\nsc-asan-auditoria2-evidencia\run-item389.cmd`, desde la
misma raíz y con esos temporales, ejecutó
`A:/nsc-asan-sdk-evidencia/build/tests/nsc_item389_native_sanitizer_tests.exe`.
Terminó con código 0 y `PASS: 10 checks`. Antes y después de ejecutarlo, el
SHA-256 del test fue
`fdaf68b0454d5e159db8d2d2f50348fc7bb39a7de31dd16d67709240666e9d4f`, y el de
`tests/unit/Item389NativeSanitizerTests.cpp` fue
`4705440ed6ac023dd1f017f1b5e48e2e14554c574ad86ee68de0bcf8b26fd1ec`.

El test comprueba el nombre y pipeline del sanitizador, la identidad declarada
del runtime Windows, compilación y lowering de un fixture, instrumentación del
módulo LLVM en **O1**, presencia de `__asan_init` y `__asan_report` en su IR y
verificación del módulo. **No ejecuta el programa NativeSharp generado** ni
demuestra el funcionamiento del driver, las bibliotecas binarias, la detección
de fugas o el comportamiento en otros niveles de optimización. Es evidencia
unitaria dirigida, no una batería completa ni una comprobación de runtime.

Los comandos, logs y hashes están en
`A:/nsc-asan-auditoria2-evidencia/result-item389.txt`, `build-item389.log`,
`run-item389.log`, `hash-item389.cmd` y `hashes-after.log`. No se ejecutó CTest
ni una suite completa.
