# Corrección del verificador de coalescencia

El caso dirigido encontró instrucciones acquire y release correctas en el IR,
pero el verificador solo aceptaba un sufijo numérico sin punto. NSC generó
`coalesce.store.fallback.193` y `.197`. Se corrige el patrón de la etiqueta sin
reducir las dos comprobaciones de carga ni de escritura. Falta repetir el caso.
