# T08 H3: control de variable de catch

El productor añade una plantilla cuya variable de excepción oculta la global C
`shadow_value`. El control existente verifica que esa global no se publique y
que el consumidor pueda ejecutar la plantilla desde la biblioteca precompilada.
Antes de la corrección, el consumidor compiló y ejecutó en O0–O3, pero el
inspector confirmó en los cuatro niveles la publicación indebida de la global.
