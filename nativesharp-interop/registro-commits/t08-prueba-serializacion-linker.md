# Tanda 8: prueba de serialización de la selección del enlazador

El validador crea un proyecto con `nsc new`, verifica que no se escriba una elección implícita de enlazador, agrega y retira un paquete de prueba y comprueba nuevamente la ausencia de la clave. Después compila y ejecuta el ejemplo TLS exigiendo selección de LLD. Como control, reescribe sidecars que eligen explícitamente NSLink y LLD y verifica que ambas elecciones sobrevivan a add/remove. La ejecución pasó en A:.
