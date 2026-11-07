# T08: propagación de fallos en controles de globales

Una copia aislada del validador global-static con un error inducido en el
productor mostró el error de compilación pero devolvió cero. Se sustituyen
las salidas afectadas por un destino de fallo explícito. También se cambia el
control de diagnóstico de const record del fixture de contratos.

Hay otros scripts con estructuras similares. Su existencia no demuestra por sí
sola que devuelvan cero: deben auditarse con fallos inducidos antes de confiar
en la validación integral. Los cambios de este registro están pendientes de
comprobación.
