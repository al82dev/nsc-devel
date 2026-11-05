# Tanda 08: referencia de serialización de restricciones

- Fallo observado: la validación agrupada informó una diferencia de bytes para `lib/NativeSharp.Core/Lazy.ns` entre los serializadores iterativo y recursivo de la prueba.
- Evidencia: el escritor y el lector de `TemplateCatalogCodec.cpp` incluyen `enclosingTypeParameter` entre `requiresRefLike` y `hasEquality`. El campo fue incorporado al formato en el commit `62ad32baa`; la referencia recursiva de `SyntaxTreeSerializationEquivalenceTests.cpp` no lo incluía.
- Cambio: la referencia de la prueba escribe ese campo en el mismo lugar y sus comentarios indican que incorpora los campos posteriores del formato. No se altera el serializador de producción ni se elimina la comprobación de igualdad.
- Validación: pendiente de repetir la validación agrupada de la tanda 08. La diferencia observada identifica una causa concreta, pero no demuestra por sí sola que no existan otras discrepancias.
