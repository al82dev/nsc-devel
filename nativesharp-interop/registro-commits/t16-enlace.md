# T16-014 a T16-016: enlace separado, símbolos duplicados y LTO

## Comprobación

Este cambio comprueba el enlace; no cambia código de producción.

## Diseño

- **Enlace separado** (T16-014): las unidades C++ se compilan por separado y se agrupan en bibliotecas; un
  módulo NativeSharp se precompila (`--emit-project-oop-lib`) y el ejecutable consume el módulo y las
  bibliotecas. Las funciones inline del encabezado (COMDAT) se eligen una vez.
- **Duplicados** (T16-015): la política es la de C++ y del enlazador: una definición fuerte repetida en dos
  entradas tomadas es un error con el nombre del símbolo; NSC no elige una. Las COMDAT se pliegan.
- **LTO** (T16-016): una biblioteca con bitcode ThinLTO se enlaza con lld-link, que NSC elige si no se fija el
  enlazador; NSLink la rechaza con su motivo. No se impone LTO.

## Evidencia

Fixture nuevo `pruebas-tanda-16-enlace/validar.py`, con y sin `--dll`: dos unidades en una biblioteca y una
tercera en otra usan una función inline del encabezado; el módulo NativeSharp precompilado las llama y el
ejecutable consume ambos, con NSLink y con lld-link. Resultado 36, igual que el control C++ de Clang 23.1.0,
O0–O3. Con `--dll` la primera biblioteca es una DLL.

Controles (estática): una biblioteca cuyo único objeto el programa necesita define otra vez `T16::clash`,
también definida en un objeto de la primera; los dos se toman y el enlace se rechaza con el nombre del
símbolo, NSLink `NSL1021 duplicate symbol definition: '?clash@T16@@YAHXZ'` y lld-link `duplicate symbol:
int __cdecl T16::clash(void)`. La biblioteca ThinLTO devuelve 42 con lld-link y NSLink la rechaza (`not
AMD64`).

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3), duplicados y LTO | `A:/temp/nsc-t16-enlace-eoene9vq` |
| DLL | pasa (O0–O3) y LTO | `A:/temp/nsc-t16-enlace-miv2sfje` |

## Límites

- NSLink no acepta bitcode; LTO entre el programa NativeSharp y la biblioteca C++ no se probó.
