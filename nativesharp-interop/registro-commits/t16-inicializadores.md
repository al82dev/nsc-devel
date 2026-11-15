# Tanda 16, cambio 1: inicializadores dinámicos que nadie ejecutaría

## Defecto

En la [auditoría inicial](../TANDA-16-AUDITORIA-INICIAL.md), un ejecutable NativeSharp que enlaza una
biblioteca estática con un entero de inicialización dinámica devuelve 0 en vez de 7, sin diagnóstico, con
NSLink y con lld-link.

Causa en el código: Clang deja el puntero al inicializador de una variable global C++ en `.CRT$XCU` (y C en
`.CRT$XIU`). Solo `_initterm`, en el arranque del runtime de C, recorre esas secciones. Sin `--crt-startup`,
la entrada `__nativesharp_start` (`WindowsRuntimeBuilder::buildStart`) llama a `Main` sin recorrerlas, y
ni `prepareExecutableBuilder` ni el enlazador inspeccionan las entradas. Una DLL NativeSharp tampoco arranca
el runtime de C.

## Diseño

Después de generar los objetos del programa y de fijar las bibliotecas del enlace, NSC busca la primera
entrada que el enlace toma con `.CRT$XCU` o `.CRT$XIU`:

- los objetos del programa, incluidos los que emite para el código C++ inline, siempre cuentan;
- de un archivo solo cuenta un miembro que define un símbolo que las entradas ya tomadas necesitan, siguiendo
  la tabla de símbolos del archivo como lo hace el enlazador, hasta el cierre;
- los miembros de importación y bitcode no cuentan.

Si hay una, el enlace se rechaza nombrando la biblioteca y el miembro. En un ejecutable, la salida es
`--crt-startup` con las bibliotecas del CRT o la biblioteca como DLL. En una DLL NativeSharp, solo la DLL.
Así, una biblioteca del CRT cuyo miembro con inicializadores no se usa (`excptptr.obj` de `libcpmt`) no se
rechaza, y las referencias al CRT de las tandas 8 y 10 siguen enlazando.

## Cambio

- `ThinLtoBackend.cpp/.h`: `findLinkedCrtInitializers` (objetos, bibliotecas) con la selección de miembros;
  sin el host estático de LLVM devuelve un error.
- `ProjectOopObjectGenerator.cpp/.h`: la expone al driver.
- `DriverArtifactGeneration.cpp`: `rejectUnstartedCrtInitializers` en el ejecutable (salvo con
  `--crt-startup`) y en la DLL NativeSharp, antes de publicar.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

[Arranque](t16-arranque.md): el negativo sin `--crt-startup` nombra `api.obj`, la DLL NativeSharp se rechaza
y una biblioteca con un miembro con inicializadores que el enlace no toma enlaza y devuelve 42 (con el
miembro tomado se rechaza nombrando `started.obj`). El fixture `pruebas-tanda-10-globales` (CRT como
referencia) pasa con el cambio (`A:/temp/nsc-t10-globales-nvvzrodc`).

Regresión: en la validación agrupada de la tanda (`TANDA-16-VALIDACION-AGRUPADA.md`).

## Límites

- La selección sigue la tabla de símbolos de cada archivo en el orden de las bibliotecas; no reproduce los
  símbolos débiles, `/include` ni lo que necesita el runtime NativeSharp, así que en esos casos puede
  diferir del enlazador.
- El TLS con inicialización dinámica (`.CRT$XDU`) no se inspecciona: sin el CRT su enlace ya falla con
  `__dyn_tls_init` y `_tls_index` sin definir.
