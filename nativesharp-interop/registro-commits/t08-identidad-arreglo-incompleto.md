# Tanda 08: identidad de arreglo C incompleto

Se prepara un modificador diferenciado para T[] de C, distinto del arreglo
flexible de un struct y del arreglo fijo. Su identidad lleva un marcador de
limite desconocido y se rechaza calcular su layout por valor; un puntero
externo mantiene layout de puntero. Falta conectar typedefs y verificar las
rutas de diagnostico, genericos y serializacion en la bateria agrupada.
