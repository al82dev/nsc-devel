# Tareas del trabajador laptop 03

## SCOPE LOCK

- Tema exclusivo: verificación de llamadas desde `#run`, caso 73.
- Rama: `development-fixes-limitaciones-laptop-03`.
- Base compartida confirmada: `71cd86d35220f654140e5619e9f9db0020f0b158`.
- Repositorio exclusivo: `A:/nsc-fixes-laptop-03`.
- Build exclusivo: `A:/nsc-fixes-laptop-03-build`.
- SDK exclusivo: `A:/nsc-fixes-laptop-03-sdk`.
- Temporales exclusivos: `A:/nsc-fixes-laptop-03-temp`.
- Cachés exclusivas: `A:/nsc-fixes-laptop-03-cache`.
- Evidencia exclusiva: `A:/nsc-fixes-laptop-03-evidencia`.

Antes de considerar vigente el caso 73, hay que reproducirlo en esta revisión con el compilador construido por este mismo clon y conservar el ejemplo original sin modificar. Una observación histórica o una corrección de otra rama no demuestra el estado de esta base.

Usar exclusivamente CMD y herramientas por nombre. No cambiar PATH ni sustituir LLVM/Clang. Cada proceso hijo debe mantener `TEMP`, `TMP`, `APPDATA` y `LOCALAPPDATA` dentro de las rutas exclusivas de este trabajador en `A:`. Después de la construcción inicial del directorio vacío, usar sólo compilaciones incrementales y pruebas dirigidas; nunca ejecutar la batería completa.

No compartir builds mutables ni copiar `CMakeCache.txt` o binarios de otra máquina. No usar `C:/native-sharp` como compilador o SDK de prueba. No modificar la copia de origen, los otros clones, fuentes de LLVM/Clang, Daikon, interoperabilidad C/C++ ni builds de interoperabilidad. `NativeSharp.Text` permanece aplazado. No desplegar ni hacer push, merge o rebase. Los defectos de linker o debugger se registran en `A:/gaps-limitaciones` y no se corrigen fuera de esta asignación.

Los cambios async sin registrar de la otra máquina no forman parte del commit base y no deben presentarse como recibidos por este clon.
