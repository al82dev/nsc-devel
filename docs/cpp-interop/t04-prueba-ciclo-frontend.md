# Ciclo de apertura y cierre de la unidad Clang

La prueba cuenta las notificaciones del consumidor de diagnósticos: una apertura
exitosa debe producir un inicio y un cierre, conservar Sema en el AST y no cerrar
de nuevo al destruirlo. El control con argumentos inválidos comprueba que se
captura el error y que las notificaciones de inicio y cierre quedan balanceadas.

La prueba se ejecutará con la tanda. El caso de argumentos inválidos no fuerza
un fallo interno de `BeginSourceFile`; ese camino también se contrasta con las
fuentes oficiales de Clang y no se debe dar por probado solo con este control.
