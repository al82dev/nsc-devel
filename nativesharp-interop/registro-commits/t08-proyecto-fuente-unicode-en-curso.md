# Proyecto y fuente Unicode: trabajo en curso

El caso de interoperabilidad con bibliotecas C precompiladas se amplió con un
proyecto en `proyecto-漢` y una fuente `Consumer-漢.ns`. La prueba reproduce
un fallo en la conversión de rutas, aun cuando los archivos existen.

Se reemplazaron conversiones de la ruta de proyecto y varias claves de fuentes
por conversiones explícitas UTF-8. La construcción incremental de NSC pasó.
La prueba dirigida confirma que ya se supera el rechazo inicial de archivo
inexistente, pero todavía falla con una excepción de conversión de caracteres.
El control C y el escenario TLS previo pasaron en la misma ejecución.

El caso sigue abierto: hay que identificar y corregir las conversiones restantes,
repetir la prueba completa de este caso y revisar las rutas de caché. No se
afirma compatibilidad general Unicode ni cierre de la tanda 8.
