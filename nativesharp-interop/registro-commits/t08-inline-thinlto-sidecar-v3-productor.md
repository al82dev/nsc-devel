# T08-003: preparar los objetos nativos del productor

El bundle conserva por separado las rutas de objetos nativos generados, además
de mantenerlas en la lista de enlace. El escritor de sidecar puede leer sus
bytes y entregarlos al codec v3; si falta un objeto o no se puede leer, falla
en vez de publicar un sidecar incompleto.

La publicación de sidecars con objetos generados sigue deshabilitada hasta
implementar y comprobar la restauración y el enlace del consumidor. El fallback
del archivo nativo continúa siendo la ruta vigente.
