# 1.0.1 — Destrucción de clases locales con base precompilada

## Resultado

El consumidor puede generar una clase local cuya cadena de herencia alcanza una
clase precompilada con destructor. El frontend ya no exige un cuerpo OOP HIR
para el destructor que pertenece a la biblioteca productora.

La entrada de destrucción local ejecuta únicamente el tramo que posee el
consumidor y después delega el tramo heredado completo mediante el descriptor
de la primera base precompilada. Esa delegación conserva los destructores y
campos `owned` privados del productor y libera la asignación exactamente una
vez.

## Defecto

`ProjectOopHirModel` omite correctamente los cuerpos de destructores
precompilados: el consumidor conoce la declaración, pero no posee su
implementación. Sin embargo, `ProjectDestructionModel` compone una cadena
semántica que incluye destructores locales y heredados. Al construir MIR para
una clase local, `ProjectOopMirModel` recorría toda esa cadena y exigía una
función HIR para cada destructor, incluido el importado.

El resultado era:

```text
Stage 35 destruction plan refers to a destructor without OOP HIR body
```

Ignorar simplemente el destructor importado habría permitido compilar, pero
habría omitido también el estado privado y los campos con ownership que sólo el
productor sabe destruir.

## Regla implementada

La cadena de destrucción se divide en una frontera de propiedad clara:

1. La entrada local ejecuta el destructor de cada clase local.
2. Libera sus campos `owned` en orden inverso de declaración.
3. Al encontrar la primera base precompilada, deja de reconstruir la cadena.
4. Obtiene el descriptor real de esa base mediante su accessor NSMETA.
5. Carga del descriptor la entrada de destrucción del productor y la invoca con
   el mismo objeto.
6. La entrada local no vuelve a liberar la asignación, porque la entrada
   delegada termina la cadena y realiza esa liberación.

La dirección del objeto es válida para ambos tramos porque el subobjeto base
comienza en el mismo encabezado de clase usado por el descriptor NativeSharp.

## Archivos y funciones

### `include/nativesharp/Frontend/ProjectOopMirModel.h`

- `ProjectOopMirDispatchTable::delegatedBaseDestructionTypeOrdinal` registra la
  primera base cuyo plan pertenece a un productor precompilado.
- El campo documenta que la operación delegada es terminal y posee la
  liberación final.

### `src/Frontend/ProjectOopMirModel.cpp`

- `ProjectOopMirModel::build()` deja de buscar cuerpos HIR para miembros
  destructores marcados `isPrecompiled`.
- La construcción ordenada de `destructionSteps` se detiene en la primera clase
  precompilada y registra su ordinal como continuación externa.
- Los destructores y campos locales anteriores a esa frontera conservan su
  orden original.

### `src/LLVM/ProjectOopLlvmLowerer.cpp`

- `defineDestructionEntries()` resuelve el accessor del descriptor de la base
  delegada.
- Carga el campo estándar de entrada de destrucción del descriptor y realiza la
  llamada indirecta con el objeto actual.
- Omite la llamada local al deallocator únicamente cuando el productor delegado
  ya posee la terminación de la cadena.

No se publica un símbolo nuevo, no se expone un destructor privado y no cambia
el formato NSMETA. Se reutilizan el accessor y el ABI del descriptor que las
bibliotecas precompiladas ya publican.

### `tests/unit/Stage46NativeLibrariesTests.cpp`

- Amplía el contrato productor/base y productor/derivado con destructores
  explícitos a ambos lados de una `LibraryReference`.
- El productor derivado reproduce el caso que anteriormente llegaba a Stage 35
  sin cuerpo HIR para el destructor de su base.
- El mismo contrato cubre biblioteca/DLL y el consumo posterior sin incluir las
  fuentes del productor base.

## Alcance

La corrección cubre cualquier profundidad de clases locales sobre una base
precompilada. Toda la porción local se ejecuta antes de delegar una sola vez la
porción externa restante.

Los destructores privados y los campos privados del productor permanecen
encapsulados. No se reconstruyen ni se publican en el consumidor.

No se modificaron SaruGL, bindings, tutoriales ni otros repositorios. Esta
entrega no requiere baseline ni ejecución de bindings.

## Commit de referencia

### Español

**Título:** Delega la destrucción de bases precompiladas

**Descripción:** Separa la cadena de destrucción en tramos local y
precompilado. Ejecuta destructores y campos owned locales, delega el resto a la
entrada almacenada en el descriptor de la primera base externa y evita exigir
cuerpos OOP HIR que pertenecen al productor.

### English

**Title:** Delegate destruction of precompiled bases

**Description:** Split the destruction chain into local and precompiled
segments. Run local destructors and owned fields, delegate the remainder to the
entry stored in the first external base descriptor, and stop requiring OOP HIR
bodies owned by the producer.

## English summary

Local classes can now inherit from precompiled classes with destructors. The
consumer destroys its locally owned segment and then calls the producer-owned
destruction entry through the imported base descriptor, preserving private
cleanup and a single final deallocation.
