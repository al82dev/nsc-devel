# Tanda 08: enlace inicial de typedef escalares

El análisis busca aliases visibles siguiendo la escala de nombres de NSC,
detecta observaciones incompatibles y colisiones con tipos nativos, y expande
los escalares que puede representar sin perder calificadores. Para los demás
emite un diagnóstico en vez de convertirlos silenciosamente. El proveedor aún
no está activado en el driver: faltan pruebas y tipos compuestos. Este avance no
cierra T08-005 y no ha pasado la validación agrupada de la tanda.
