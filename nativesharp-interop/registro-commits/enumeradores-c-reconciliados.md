# Enumeradores C reconciliados

Cuando un enum fue reconciliado por el cierre de un record comprobado, su
enumerador vivo puede señalar al enumerador equivalente de la biblioteca tras
verificar USR, valor y tipo. La resolución solo unifica candidatos cuando ambos
son visibles en esa referencia; los nombres ajenos conservan su ambigüedad.
La construcción y validación agrupadas aún están pendientes.
