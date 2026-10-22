# Configuración opcional del directorio canónico de import libraries

## SCOPE LOCK

- Objetivo: permitir compilar `WindowsRuntimeImports.cpp` cuando el consumidor no proporciona la ruta instalada.
- Naturaleza: corrección no funcional de compatibilidad de construcción introducida por T7.5.
- Revisión fallida: `0ebdbae8c`.
- Productor responsable: fallback de `canonicalImportLibraryDirectory`.
- Archivo de producto autorizado: `src/Linker/WindowsRuntimeImports.cpp`.
- Protegidos: pruebas, fixtures, CMake, NSC Builder, sintaxis, semántica, diagnósticos, CLI, ABI del lenguaje,
  NSMETA, HIR, MIR, LLVM IR y artefactos emitidos.
- Validación autorizada: limpieza, compilación y ejecución de la batería unitaria completa.
- Prohibido: pruebas externas, minibaseline, baseline, instalación y deploy.
- Terminación: el producto compila con y sin definición instalada y la batería unitaria completa pasa.

## Evidencia

La construcción limpia de `nsc_component_tests` llegó a 758 de 784 objetos y falló exclusivamente en
`WindowsRuntimeImports.cpp:80`: `NSC_DEFAULT_WINDOWS_RUNTIME_IMPORT_LIBRARY_DIRECTORY` no estaba declarada.

El commit `eace24576` añadió el fallback y su definición privada al target `nsc_linker`. El suite unitario compila
el fuente de producto directamente, por lo que no hereda esa definición. La variable de entorno y la búsqueda
relativa al ejecutable ya preceden al fallback instalado.

## Corrección

El fallback instalado se usa sólo cuando su macro está definida. Cuando un consumidor directo no configura esa
ruta, la función devuelve una ruta vacía después de agotar el override y la búsqueda relativa. La función pública
conserva su validación de archivos y su diagnóstico canónico de import library ausente.

## Contratos conservados

- El target productivo `nsc_linker` conserva exactamente su ruta instalada configurada por CMake.
- Se conservan la precedencia del override de entorno y la detección relativa al ejecutable.
- No se generan import libraries durante el enlace ni se modifica su tabla de símbolos.
- No cambian sintaxis, semántica, diagnósticos de compilación, CLI, ABI del lenguaje, NSMETA ni artefactos.
- No se modifica ninguna prueba ni archivo CMake.

## Validación

Estado actual: `CORRECCIÓN COMPILADA, EJECUCIÓN UNITARIA COMPLETA PENDIENTE`.

Se limpió `build-compiler-suite-v3/debug`: Ninja eliminó 757 archivos y terminó con código 0. En la reconstrucción
desde cero, `WindowsRuntimeImports.cpp` compiló en el paso 748 de 784. La macro ausente no volvió a producir un
diagnóstico y se compilaron los 784 objetos.

El enlace final se detuvo por una causa independiente: no se encontró la definición de producto
`createCompilationInputManifest(...)`. Por ello la ejecución unitaria completa continúa pendiente; esa causa no
pertenece a esta hipótesis ni a este commit.
