# Globales C internas en proyectos fuente

El consumidor de `static-dos-bibliotecas-mutables` también se compiló con
`ProjectReference` a ambos productores fuente, en lugar de sus bibliotecas
precompiladas. Las dos globales `static` homónimas conservaron sus valores
independientes tras lecturas y modificaciones, en O0.

El caso se reproduce con `validar-fuente.cmd`. No demuestra por sí solo las
demás formas de resolución de identidad ni cierra T08-002.
