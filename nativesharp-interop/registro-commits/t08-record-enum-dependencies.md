# Tanda 08: dependencia de enum desde campos de records C

Al seleccionar un record C, el selector incorpora los enums importados que
aparecen en sus campos, incluidos los detrás de punteros o arreglos. Un enum
con definiciones conflictivas produce error. La selección de dependencias de
funciones y las colisiones con tipos nativos siguen pendientes; esta ruta no se
ha compilado ni ejecutado.
