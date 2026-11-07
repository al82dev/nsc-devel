# T08-004: diagnostico exacto de escritura const

La ejecucion mostro `error: left side of assignment is not assignable`.
El control buscaba `readonly`, que tambien figuraba en la ruta del archivo;
ahora busca el diagnostico exacto y debe ejecutarse de nuevo.
