# Tanda 17, cambio 3: símbolo ausente frente a declaración distinta

## Defecto

En la [auditoría inicial](../TANDA-17-AUDITORIA-INICIAL.md), una función que el encabezado declara y ninguna
biblioteca define, y una función que el encabezado declara `one(int)` y la biblioteca define
`one(long long)`, terminan con el mismo mensaje del enlazador (`unresolved external symbol: '?one@A@@YAHH@Z'`),
sin la declaración ni la definición parecida.

Causa en el código: el driver entrega el enlace a NSLink o lld-link y solo reenvía su salida. Una diferencia
de tipos, convención o calificadores cambia la decoración de Microsoft, así que para el enlazador es un
símbolo distinto.

## Diseño

Cuando el enlace de un ejecutable o una DLL NativeSharp falla, el driver revisa cada función C/C++ que llama
el programa (de su MIR):

- falta si ningún objeto la define y ninguna biblioteca la da, directamente o como `__imp_` de una DLL;
- si falta, se nombra con su forma legible, su símbolo y la ubicación de su declaración, y se dice que no la
  define ninguna biblioteca;
- si una biblioteca define el mismo nombre calificado C++ con otra decoración, se dice que el encabezado y la
  biblioteca discrepan en la declaración, con una nota por definición (biblioteca, forma legible y símbolo).

La revisión corre solo después de un enlace fallido: no cambia un enlace correcto. El mensaje del enlazador
se conserva antes.

## Cambio

- `ThinLtoBackend.cpp/.h`: `findMissingLinkSymbols` y `demangleSymbol` (desdecoración de Microsoft de LLVM);
  sin el host estático de LLVM devuelven un error y el símbolo.
- `ProjectOopObjectGenerator.cpp/.h`: los exponen al driver.
- `DriverArtifactGeneration.cpp`: `explainMissingCppSymbols` tras un enlace fallido de ejecutable o DLL.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture `pruebas-tanda-17-diagnosticos` (registro de [diagnósticos](t17-diagnosticos.md)). Sonda con el
compilador del cambio (`A:/temp/nsc-t17-sonda-y4droiwm`):

```
nsc: error: C/C++ function 'int __cdecl A::two(void)' (symbol '?two@A@@YAHXZ'), declared at .../api.h:2:39, is not defined by any linked library
nsc: error: C/C++ function 'int __cdecl A::one(int)' (symbol '?one@A@@YAHH@Z'), declared at .../api.h:2:19, is not defined with this declaration: the header and the library disagree on it
nsc: note: '...\api.lib' defines 'int __cdecl A::one(__int64)' (symbol '?one@A@@YAH_J@Z')
```

Un encabezado o una biblioteca ausentes ya se distinguían (T17-005): `CppLibraryReference header does not
exist` y `referenced library does not exist`.

## Límites

- Una función C (`extern "C"`) no tiene decoración: falta o no, sin la definición parecida.
- Una variable o un miembro C++ que falta queda con el mensaje del enlazador.
