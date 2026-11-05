# Conexión de acción y cancelación

La apertura usa la acción privada de Clang y un sistema de archivos que consulta
el token. Una cancelación detectada al cargar la entrada se informa como tal; se
mantiene la comprobación posterior al análisis para no devolver una sesión viva
después de una cancelación.

Los nuevos componentes se incorporan al target opcional de interoperabilidad.
La validación se ejecutará al terminar la tanda.
