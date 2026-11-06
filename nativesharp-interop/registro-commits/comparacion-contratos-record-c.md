# Comparación de contratos C entre módulos

Se prepara una comparación de snapshots con el codec validado: conserva destino,
USR, dimensiones, campos y calificadores, y normaliza únicamente la identidad
del módulo para comparar los datos de declaración. Un record incompleto o sin
USR proporciona evidencia insuficiente. El resultado coincidente no demuestra
procedencia común ni habilita la conversión entre identidades.

La construcción incremental de `nsc_cpp_session_tests` y la ejecución de la
suite terminaron con código 0. La compilación emitió 29 advertencias de campos
sin inicializador en el archivo de pruebas y otras en código existente. Todavía
no se utiliza para permitir el caso compartido ni modifica el formato NSMETA.
