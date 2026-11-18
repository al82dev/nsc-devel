# Tanda correctiva 07: edicion fuente y cierre self-host O0

> **Vigencia:** NativeSharp es un lenguaje único y acumulativo; el producto es 1.0. Las referencias históricas a ediciones o versiones de lenguaje y NSMeta que aparezcan a continuación no definen compatibilidad ni deben reintroducirse.

## SCOPE LOCK histórico

- Se corrige exclusivamente la contaminacion semantica causada al inferir la
  edicion efectiva de un grafo con proyectos que omitieron `<Language>`.
- No se modifican proyectos, fuentes ni pruebas historicas.
- No se relajaban las reglas elegidas explícitamente en la configuración de entonces.
- No se modifica ni instala nada en `C:`.
- La validacion es dirigida y exclusivamente O0; no se ejecutan baseline,
  minibaselines ni matrices O1/O2/O3.

## Causa raiz

Las tandas 2 y 3 hicieron que un proyecto implicito adoptara la unica edicion
del grafo para resolver referencias fuente y NSMETA sin un falso `NS3001`. Ese
valor efectivo se reutilizaba tambien para analizar sus fuentes. Por ello,
`NativeSharp.Compiler.nsproj`, que historicamente omite `<Language>`, quedaba
optado silenciosamente a las reglas de efectos de su dependencia Core. El
self-host producia mas de cien `NS3030` en llamadas LLVM que eran
validas bajo la semantica historica seleccionada por la omision.

La edicion necesaria para que el grafo sea coherente y la edicion que eligio el
autor para sus fuentes son contratos distintos. Inferir la primera no autoriza
cambiar la segunda.

## Cambio histórico

La corrección de entonces separaba la etiqueta fuente de la inferida para el
grafo y transmitía la procedencia por proyecto al lexer/parser, los cuerpos,
los shards, los inicializadores y la validación de `throws`. Conservaba el
diagnóstico de `unsafe` para la configuración explícita de la campaña.

**Contrato vigente:** esa selección por edición fue retirada. Todos los
proyectos usan el lenguaje único y las reglas de efectos aplicables; la
evidencia siguiente corresponde sólo a aquella revisión.

## Archivos

- `include/nativesharp/Compilation/Project.h`
- `src/Compilation/Project.cpp`
- `include/nativesharp/Frontend/ProjectMethodBodyBuildOptions.h`
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
- `src/Frontend/ProjectCompilation.cpp`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `tests/unit/ImplicitLegacyLanguageSemanticsTests.cpp`: prueba nueva; no se
  modifica ninguna prueba historica.
- `CMakeLists.txt`: registra solamente la prueba nueva.

## Evidencia dirigida

Pasaron juntas cuatro pruebas:

- `nsc.compilation.language-edition-inference`;
- `nsc.compilation.precompiled-language-edition-inference`;
- `nsc.compilation.implicit-language-edition-semantics`;
- `nsc.ns3.t13.unsafe-auditing`.

Resultado: 4/4 en 1.04 s.

Con el `nsc.exe` construido en `A:\nsc-devel\build-test-nslink\bin`,
`NS_LIB_ROOT=A:\native-sharp-3.0-baseline\lib` y
`NSC_LINKER=A:\nslink-devel\bin\Release\nslink.exe`, el proyecto self-host se
emitio en O0 correctamente:

- 101 fuentes;
- method-bodies: 168 ms;
- 11 unidades LLVM;
- enlace NSLink: correcto;
- salida: `build-test-nslink/_targeted/selfhost-o0/nsc-next-o0.exe`.

La imagen derivada produjo correctamente:

- `--dump-source-graph`: 5 proyectos, 101 unidades y orden contractual;
- `--dump-module-set`: 27 unidades raiz, 4 proyectos externos, 0 problemas;
- `--check-project`: 101 unidades de grafo, 27 comprobadas y 0 fallos.

No se ejecuto baseline, minibaseline ni ningun perfil distinto de O0.
