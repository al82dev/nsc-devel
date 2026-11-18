# NativeSharp: sincronizacion de la stdlib canonica

## Problema

La instalacion del compilador publicaba las fuentes de `A:\nsc-devel\lib`. Ese arbol contenia una
revision anterior de la biblioteca estandar, aunque `C:\native-sharp` se hubiera eliminado antes de
instalar. Por eso una instalacion limpia restauraba, entre otros casos, `HashSet.RemoveWhere` con el
parametro reservado `match`. Los proyectos ya no necesitan seleccionar una versión del lenguaje.

## Cambio

Se sincronizaron completos desde `A:\nativesharp-libraries\stdlib` los modulos canonicos:

- `NativeSharp.Core`;
- `NativeSharp.Core.Threading`;
- `NativeSharp.Core.Collections.Generic`.

La sincronizacion incluye proyectos, fuentes y documentos propios de cada modulo. Tambien elimina
los archivos que ya no existen en la fuente canonica, para impedir una mezcla parcial de versiones.

## Mejora

Una instalacion nueva del compilador ya parte de los mismos modulos que mantienen las
bibliotecas oficiales. El empaquetador de bindings no necesita adaptar sintaxis ni seleccionar una
stdlib alternativa.

## Verificacion dirigida

- inventario relativo identico entre cada modulo canonico y su copia vendorizada;
- SHA-256 identico para cada archivo correspondiente;
- los tres proyectos usan el lenguaje acumulativo sin seleccionar versión;
- `HashSet.RemoveWhere` usa `predicate`, no la palabra reservada `match`.

No se ejecuto baseline, build completo ni configuraciones O1, O2 u O3.
