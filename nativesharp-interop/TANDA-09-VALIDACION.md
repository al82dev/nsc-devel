# Validacion de la tanda 9 en Windows x64

Las pruebas nuevas usan Clang 23.1.0 disponible por nombre, herramientas LLVM
del mismo entorno y un compilador NSC de desarrollo. Sus artefactos y registros
se crean en directorios individuales bajo `A:/temp`; no se modifica PATH.
Para los proyectos ejecutables, el entorno debe seleccionar NSLink y la
biblioteca runtime de la construccion. En la laptop se usaron `NSC_LINKER` con
el nombre `nslink.exe` del PATH y `NSC_WINDOWS_RUNTIME_LIBRARY` con
`A:/nsc-devel/build-tanda09/generated/NativeSharp.Runtime.Win32.lib`.

Ejecutar `validar.py` en cada uno de estos directorios, pasando `--nsc` con el
compilador de desarrollo y `--clang-root` con la raiz de Clang:

- `pruebas-tanda-09-llamadas`, `pruebas-tanda-09-agregados` y
  `pruebas-tanda-09-vectorcall`: ocho comparaciones C/NS cada uno, biblioteca
  estatica y DLL en O0–O3.
- `pruebas-tanda-09-global-const` y `pruebas-tanda-09-simbolo-ausente`:
  diagnosticos negativos C y NS.
- `pruebas-tanda-09-precompilado`: 16 consumidores de una biblioteca NS, con
  y sin encabezados C, estatica y DLL en O0–O3.
- `pruebas-tanda-09-cache` y `pruebas-tanda-09-cache-biblioteca`: controles de
  reutilizacion e invalidacion en O0.

`pruebas-tanda-09-biblioteca-ausente/validar.py` solo requiere `--nsc` y
comprueba que el diagnostico incluya la ruta de la biblioteca inexistente. Cada
validador comunica su directorio de evidencia y termina con error ante un paso
inesperado. Consultar [la auditoria](TANDA-09-AUDITORIA-INICIAL.md) para los
resultados de esta sesion y sus limites.

Adicionalmente pasaron seis controles CTest seleccionados: project-loader,
item331, item332, item333, item344 e item345. Paso tambien la prueba aislada de
identidad C. No se ejecutaron los gates completos de la tanda 8 ni la suite de
sesion Clang, cuya entrada `cpp-fixtures/project/CppProbe.nsproj` falta en esta
laptop. `stage48_2` no se ejecuto y permanece intacta; su fixture historico
ausente no constituye un requisito nuevo de versionamiento o compatibilidad.
