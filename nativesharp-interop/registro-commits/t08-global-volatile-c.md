# Tanda 08: global C volatile

Se prepara acceso desde NativeSharp a un global C volatile y se comprueba
mediante C. El script distingue en LLVM las lecturas y escrituras volatiles
del campo y del global, para evitar que un caso enmascare al otro. Esta nueva
version del proyecto y sus comprobaciones aun no se han ejecutado.
