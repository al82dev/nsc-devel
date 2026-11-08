# T08 H3: captura preparada de global C seleccionada

Se prepara la captura de una global C ya resuelta, comprobando proyecto, referencia, destino, nombre, USR, simbolo de enlace, TLS y modo volatile. Para una global interna se comprueba el simbolo sintetico usado por el productor. No se instala ni publica la global y se rechazan observaciones ambiguas sin alterar la salida.

Se prepararon controles sinteticos de identidad, TLS, referencia equivocada y duplicados; aun no se compilaron ni ejecutaron. Falta conectar selecciones reales, comprobar aliases y sombras, conservar la inicializacion y enlace interno de plantillas, y completar publicacion y restauracion. El cambio no cierra H3 ni la tanda 8.
