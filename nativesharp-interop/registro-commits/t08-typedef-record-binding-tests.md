# Tanda 08: prueba integrada de typedef a record

La prueba preparada inyecta el record C `Pair`, su alias del mismo nombre y un
alias alternativo `PairAlias`; analiza un archivo NativeSharp que usa ambos como
punteros y comprueba la expansión al record importado. Aún no se ha ejecutado y
no acredita construcción, destrucción ni paso del record por valor.
