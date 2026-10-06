# NativeSharp 3: contrato de instalacion modular de stdlib

## Problema

El instalador del compilador copiaba `lib/` completo y generaba su manifiesto desde ese mismo arbol.
Eso publicaba `NativeSharp.Core.IO` y `NativeSharp.Core.Diagnostics` como si continuaran formando
parte de Foundation, aunque la stdlib canonica de Language 3 ya esta dividida en tres modulos.

## Cambio

La distribucion del compilador declara ahora un inventario explicito:

- `NativeSharp.Core`;
- `NativeSharp.Core.Collections.Generic`;
- `NativeSharp.Core.Threading`;
- `NativeSharp.LLVM.C`;
- `NativeSharp.Win32.Compiler`.

El manifiesto y las reglas de instalacion usan el mismo inventario. El verificador de instalacion ya
exige solamente los tres modulos publicos de la stdlib. `NativeSharp.Core.IO` y
`NativeSharp.Core.Diagnostics` permanecen en el repositorio para pruebas internas y self-host
historico, pero no se vuelven a publicar en una instalacion nueva.

## Archivos modificados

- `CMakeLists.txt`;
- `src/Driver/InstallationVerifier.cpp`;
- `tests/unit/Stage63InstallationVerifierTests.cpp`.

## Mejora

La instalacion limpia y el manifiesto coinciden con la composicion modular real de Language 3. Una
actualizacion administrada puede retirar los dos modulos obsoletos sin que la siguiente instalacion
los restaure.

## Verificacion dirigida

- el catálogo CMake no enumera `Core.IO` ni `Core.Diagnostics`;
- el verificador exige exactamente los tres proyectos stdlib actuales;
- la fixture mínima de Stage 63 representa el mismo contrato.

No se ejecuto baseline, build completo ni configuraciones O1, O2 u O3.
