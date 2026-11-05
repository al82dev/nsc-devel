# Tanda 08: estado semántico de enumeradores C

Un `NamespaceStateSymbol` puede identificar una constante entera C importada sin
tratarla como variable externa. El índice valida que tenga inicializador entero,
tipo y nombre de almacenamiento generado, y el análisis de expresiones no permite
tomar su dirección. El flujo de producción aún no suministra estas constantes;
la compatibilidad con expresiones constantes y la validación están pendientes.
