# Alcance de puntero volatile obtenido de arreglo C

Se acota el transporte del modo MS al caso de un puntero a int volatile
cargado directamente de un arreglo global C. La escritura del slot y la del
entero apuntado son operaciones diferentes. Los casos indirectos permanecen
abiertos y la validación se agrupará con la tanda.
