# T08-015: diagnósticos de dirección const

El fixture se detuvo porque esperaba `addressable storage` al rechazar el
campo readonly de un record; NSC emitió el diagnóstico específico `readonly
storage`. El caso enum, ejecutado por separado, fue rechazado al convertir el
puntero por pérdida de const. El script comprueba ahora ambos diagnósticos
específicos. Falta volver a ejecutar la validación agrupada.
