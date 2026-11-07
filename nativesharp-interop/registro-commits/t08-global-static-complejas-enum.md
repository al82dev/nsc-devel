# T08-004: observar enum de global C interna demandada

Tras habilitar el arreglo interno, su tipo enum seguia sin proyectarse porque
la observacion de enums de globales solo consideraba enlace externo. Se incluye
la declaracion C interna demandada en esa observacion. Construccion y
validacion de la matriz permanecen pendientes.
