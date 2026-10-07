# Tanda correctiva 04: superficie publica de Core fuera de alcance

## Resultado

Las fallas atribuidas inicialmente a la superficie publica de Core no proceden
de la reingenieria semantica ni de la optimizacion de `method bodies`.

El commit independiente `9232bcfc0` (`Sync bundled stdlib with Language 3
sources`) elimino de la copia incluida en este repositorio:

- `lib/NativeSharp.Core.Threading/ReferenceCounting.ns`;
- `lib/NativeSharp.Core/BorrowedReference.ns`;
- los overloads escalares y la familia `Console.ReadLine` de
  `lib/NativeSharp.Core/Console.ns`.

Las pruebas historicas T10, T30 y T31 consumen o inspeccionan esas superficies.
Por tanto, sus fallas no pueden corregirse en el productor del compilador: los
simbolos ya no existen en las fuentes de biblioteca que reciben esas pruebas.

## Decision de alcance

Esta tanda queda **clasificada, no corregible dentro de la optimizacion del
compilador**. No se modifica ninguna prueba, fixture, proyecto, biblioteca ni
runtime para ocultar el problema.

La restauracion, sustitucion o migracion de esa API pertenece a una tarea de
bibliotecas separada. Incluirla aqui mezclaria un cambio de superficie publica
con la correccion de regresiones del compilador e impediria atribuir los
resultados.

## Evidencia estatica

- `git show 9232bcfc0` registra la eliminacion completa de
  `ReferenceCounting.ns` y `BorrowedReference.ns`.
- El mismo commit elimina 77 lineas de `Console.ns`, incluidos los overloads y
  `ReadLine`.
- `NativeSharp3T10ReferenceCountTests.cpp`,
  `NativeSharp3T30ConsoleInputTests.cpp` y
  `NativeSharp3T31ArrayBorrowTests.cpp` conservan el contrato historico.
- La diferencia no forma parte de los commits de reingenieria semantica.

## Validacion

No se ejecuto ninguna prueba: una prueba dirigida no puede producir esos
simbolos ausentes y no se autoriza modificar las bibliotecas durante esta
serie. La conclusion se obtiene de la historia exacta y de los consumidores
inmutables.

