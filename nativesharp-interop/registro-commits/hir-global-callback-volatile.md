# Orden MS para global C volatile de callback

El HIR reconoce la identidad estructural de función como puntero al determinar
el ancho para un global C volatile. El orden solo se habilita con el modo MS,
tamaño y alineación ya comprobados. Quedan pendientes las pruebas y el manejo
equivalente de campos.
