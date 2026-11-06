# Proyección de global C volatile struct

La proyección conserva `volatile` en el estado global de un struct C completo
con layout verificado. Los punteros a volatile y arreglos de records calificados
siguen sujetos a sus barreras. El acceso requiere el transporte HIR y pruebas.
