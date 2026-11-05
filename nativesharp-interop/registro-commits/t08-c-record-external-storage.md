# Almacenamiento externo de records

La validación encontró que LLVM no tenía carrier general para el struct C
importado. La declaración del global externo usa su tamaño físico verificado
y las dimensiones de arreglo, como almacenamiento de bytes. Esta ruta no
crea una representación general para pasar, construir o copiar records por
valor. El acceso a campos y el indexado siguen pendientes de validación.
