# Ruta de selección de LLD para TLS

## Hecho observado

El proyecto `CTlsPrerequisite` enlaza y ejecuta con LLD cuando se establece
`NSC_LINKER`, pero con NSLink falla en el enlace por `NSL1023`; ver
`tls-estatico-control-original.md`. En el comando `nsc build`, pedir
`--linker LLD` sin configurar `NSC_LINKER` se rechaza en
`src/Driver/ProjectCommands.cpp`, función que selecciona los parámetros de
construcción. Esta no es todavía detección automática de TLS.

## Cadena vigente

1. `ProjectCommands.cpp` calcula `linker`, verifica coherencia con el entorno
   y lo reporta en JSON. `executeConfiguredBuild` construye la invocación
   interna, pero no transmite allí el nombre seleccionado.
2. `CompilerInvocation.h`, `CompilerOptionParser.cpp` y
   `CompilerActionDispatcher.cpp` transportan opciones hasta
   `Driver::runProjectOopExecutableGeneration` y la generación de DLL.
3. `DriverArtifactGeneration.cpp` configura `WindowsExecutableBuilder` o
   `WindowsLibraryBuilder`; hoy no les pasa una selección explícita.
4. `WindowsExecutableBuilder.cpp`, `WindowsLibraryBuilder.cpp` y
   `PrecompiledModuleConfiguration.cpp` llaman a `LldLinker::discover`.
   `LldLinker.cpp` lee `NSC_LINKER` o busca NSLink. Los constructores y la
   configuración de módulos precompilados también usan la herramienta en
   sus identidades de caché.

## Cambios necesarios antes de implementarlo

- Resolver una sola selección por invocación y transmitirla como dato, no
  modificar el entorno global del proceso ni el código de NSLink.
- Mantener los rechazos existentes cuando el usuario proporciona explícita y
  contradictoriamente `NSC_LINKER` y `--linker`; están cubiertos por
  `pruebas-tanda-08-tls-c/lld/validar-seleccion.cmd`.
- Propagar la selección por el comando interno, el despacho, productores de
  ejecutables y bibliotecas y la preparación de módulos precompilados.
- Usar la misma ruta resuelta para enlazar y calcular identidades de caché;
  comprobar además bibliotecario, referencias C/C++ y `--print-linker`.
- Comprobar selección explícita de LLD sin `NSC_LINKER`, selección explícita
  de NSLink, conflictos, herramienta ausente y construcción incremental.
- La detección automática de TLS dentro de objetos o bibliotecas, incluidos
  símbolos internos que no aparecen en el header, es un trabajo adicional;
  no debe confundirse con permitir `--linker LLD` sin variable de entorno.

Este documento registra la ruta causal; no es una implementación ni afirma
que la tanda 8 esté validada. Todos los cambios deben quedar en NSC y en A:.
