# Definiciones tentativas C en el módulo inline

El primer enlace dejó `shared_state` sin definición porque la generación aislada no notificaba las definiciones tentativas. Se usa la definición activa indicada por Clang y `CompleteTentativeDefinition`, preservando el estado interno compartido. Se vuelve a validar el grupo completo.
