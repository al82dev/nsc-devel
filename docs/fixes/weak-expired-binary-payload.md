# Referencia débil expirada con carga binaria

La regresión de carga binaria comprueba que, tras liberar el último propietario
de `SafePayload`, `WeakRc` y `WeakArc` informan expiración, `Upgrade()` devuelve
un propietario vacío y el destructor se ha ejecutado exactamente una vez.
Los proyectos temporales de la prueba no declaran una versión de lenguaje.

Las variantes de biblioteca estática y DLL NativeSharp pasaron en O0 y O2 con
el compilador y Core aislados en A. También pasaron sus controles de cargas y
rechazo de publicación del receptor. Registros:
`A:/nsc-fixes-evidencia/pointers-static-weak-expired.log` y
`A:/nsc-fixes-evidencia/pointers-dll-weak-expired.log`. El compilador y el SDK
proceden del árbol con otros cambios locales; véase
`A:/nsc-fixes-evidencia/pointers-sdk-refresh-scope.md`.

En la variante dinámica la DLL permanece cargada durante toda la ejecución.
La prueba no descarga el módulo ni demuestra que una referencia débil conserve
su vida después de una solicitud de descarga. Esa parte sigue pendiente.
