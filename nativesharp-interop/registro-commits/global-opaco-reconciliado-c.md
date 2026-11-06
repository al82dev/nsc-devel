# Global C de puntero opaco reconciliado

La primera prueba con procedencia conservada superó el conflicto de alias pero
rechazó `opaque_current` y `union_current`: el global exigía propietario local
aunque la identidad apuntaba a un tipo precompilado reconciliado. Se prepara
el uso de la identidad canónica cuando la conciliación ya está acreditada,
conservando las comprobaciones de tipo incompleto, origen C, USR y errores.
La construcción y validación del cambio todavía están pendientes.
