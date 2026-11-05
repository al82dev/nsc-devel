# Tanda 08: referencia de serialización de restricciones

- Fallo observado: la validación agrupada informó una diferencia de bytes para `lib/NativeSharp.Core/Lazy.ns` entre los serializadores iterativo y recursivo de la prueba.
- Evidencia: el escritor y el lector de `TemplateCatalogCodec.cpp` incluyen `enclosingTypeParameter` entre `requiresRefLike` y `hasEquality`. El campo fue incorporado al formato en el commit `62ad32baa`; la referencia recursiva de `SyntaxTreeSerializationEquivalenceTests.cpp` no lo incluía.
- Cambio: la referencia de la prueba escribe ese campo en el mismo lugar y sus comentarios indican que incorpora los campos posteriores del formato. No se altera el serializador de producción ni se elimina la comprobación de igualdad.
- Validación: `call nativesharp-interop\validar-tanda-08.cmd`, ejecutado el 2026-11-05 desde `A:/nsc-devel`, terminó con código 0. La comparación recursiva/iterativa pasó sus cuatro comprobaciones, la restauración de sintaxis pasó seis y también pasaron las pruebas de sesión Clang y los siete proyectos `valores`, `incompleto`, `record-c`, `union`, `layout`, `callback` y `enum-funcion`.
- Alcance: este resultado corrige el fallo observado y valida los casos de esa ejecución; no prueba por sí solo todos los contratos pendientes de la tanda 08.
