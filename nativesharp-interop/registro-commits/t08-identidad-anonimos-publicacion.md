# Publicación de identidades de anónimos anidados

`anonimos-anidados/verificar_usr.py` compara las cadenas de USR reportadas por
`c-index-test` con los datos de la biblioteca que acaba de generar NSC. Los dos
campos y los dos enumeradores de la muestra están presentes; el validador pasó
en O0, con ejecución directa y precompilada. Esta búsqueda confirma presencia
de cadenas, no demuestra que el lector las asocie a los miembros correctos.
La comparación de las identidades proyectadas y restauradas sigue pendiente.
