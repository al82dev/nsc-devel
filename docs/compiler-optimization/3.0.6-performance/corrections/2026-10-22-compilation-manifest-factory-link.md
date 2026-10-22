# Enlace de la fábrica de manifiestos de compilación

## SCOPE LOCK

- Objetivo: hacer que todo consumidor de `ProjectCommands.cpp` enlace la definición declarada de
  `createCompilationInputManifest`.
- Naturaleza: corrección no funcional de composición y modularidad del producto introducida por la reutilización
  de digests.
- Revisión fallida: `ba9f2fa42`.
- Productor responsable: ubicación de la definición de `createCompilationInputManifest`.
- Archivos de producto autorizados: `CompilationInputManifestFactory.cpp`, `PrecompiledModuleConfiguration.cpp`
  y `CMakeLists.txt` raíz.
- Infraestructura unitaria autorizada: añadir exclusivamente el nuevo fuente de producto a
  `tests/compiler-suite/CMakeLists.txt`.
- Protegidos: casos, expectativas, fixtures, NSC Builder, sintaxis, semántica, diagnósticos, CLI, ABI del lenguaje,
  NSMETA, HIR, MIR, LLVM IR y artefactos emitidos.
- Validación autorizada: limpieza, compilación y ejecución de la batería unitaria completa.
- Prohibido: pruebas externas, minibaseline, baseline, instalación y deploy.
- Terminación: el ejecutable unitario enlaza la fábrica y la batería unitaria completa pasa.

## Evidencia

Los 784 objetos de `nsc_component_tests` compilaron, pero `lld-link` no encontró
`nativesharp::driver::createCompilationInputManifest(ProjectGraph, ProjectLoadOptions)`, referenciado por
`ProjectCommands.cpp:1573`.

El header `Internal/CompilationInputManifestFactory.h` declara la función y `ProjectCommands.cpp` la consume. Sin
embargo, el commit `c12060aa1` dejó su cuerpo dentro de `PrecompiledModuleConfiguration.cpp`; el target unitario
compila el primer consumidor pero no ese módulo no relacionado.

## Corrección

Se mueve literalmente la fábrica y su helper privado a `CompilationInputManifestFactory.cpp`, coherente con el
header ya existente. El nuevo fuente se añade al target productivo y a la lista explícita con la que el suite
compone el producto. No se añade ni modifica ningún caso de prueba.

## Contratos conservados

- Se conservan exactamente los ocho campos comunes de identidad y su orden.
- `PrecompiledModuleConfiguration.cpp` sigue llamando a la misma función declarada.
- `ProjectCommands.cpp` enlaza la misma implementación compartida; no duplica la identidad.
- No cambian sintaxis, semántica, diagnósticos, CLI, ABI del lenguaje, NSMETA, IR ni artefactos.
- La única modificación en infraestructura unitaria es una ruta de fuente de producto.

## Validación

Estado actual: `PRODUCTOR UNITARIO COMPILADO Y ENLAZADO, EJECUCIÓN COMPLETA PENDIENTE`.

CMake regeneró correctamente el target y Ninja limpió 784 archivos. La reconstrucción desde cero compiló el nuevo
`CompilationInputManifestFactory.cpp` en el paso 679, `ProjectCommands.cpp` en el paso 688 y enlazó
`nsc_component_tests.exe` en el paso 785 de 785 con código 0. El símbolo faltante quedó resuelto.

La siguiente acción es verificar que `--list` conserve los 3,149 casos canónicos y ejecutar `--all`. La batería
unitaria completa todavía no se declara pasada hasta analizar esa salida.
