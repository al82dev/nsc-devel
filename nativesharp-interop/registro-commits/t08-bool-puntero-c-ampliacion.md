# Tanda 8: control cruzado de punteros bool

Se amplía el caso para que C modifique un bool local de NativeSharp, para que C lea ese mismo almacenamiento después de una escritura nativa y para que una función NativeSharp seleccione entre punteros de ambos orígenes. El verificador exige ahora accesos de byte en IR además de la ejecución cruzada.

Esta ampliación acompaña una corrección separada; por sí sola no acredita su resultado ni cierra el ítem.
