# Despacho e identidad de la selección de LLD

La selección de la invocación interna se transmite a las rutas de producción
de ejecutables, bibliotecas estáticas y DLL, y a la identidad de herramientas
del bundle precompilado. Los generadores de artefactos reciben la selección
como dato de su propia instancia. Una selección vacía conserva el mecanismo
anterior; no se cambia NSLink ni el entorno global.

El comando `build` aún debe transmitir la selección interna y permitir LLD
sin `NSC_LINKER`. La construcción y la batería agrupada están pendientes;
este commit por sí solo no habilita la funcionalidad pública.
