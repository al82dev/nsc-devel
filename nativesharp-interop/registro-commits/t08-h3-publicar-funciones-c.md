# Tanda 8: publicar funciones C candidatas de plantillas

El productor agrega al catálogo 1.46 las funciones C externas proyectadas que
coinciden con demandas de plantillas públicas. La captura conserva módulo,
referencia, USR y símbolo. Todavía no representa funciones internas ni demandas
que no se pudieron proyectar, ni prueba que cada candidato haya sido elegido.

La prueba de biblioteca real pasó en O0 junto con el driver y el verificador;
no demuestra compatibilidad general ni cierra la tanda.
