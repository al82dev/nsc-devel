# Caché de compilación con encabezados C directos

El driver solicita a cada sesión C la huella del preprocesador y los contenidos
observados, y los incorpora junto con sus opciones y la herramienta Clang a la
identidad de la compilación. Comprueba que el grafo final corresponda a las
sesiones abiertas. Si falta evidencia, se detecta una macro inestable, o hay
una sesión C++, no se consulta ni se publica la caché de compilación completa.
La compilación continúa sin ese atajo.

`cache-c-directa/validar.py` compiló y ejecutó en O0 y O3: primera compilación,
acierto posterior, aparición de un archivo detectado con `__has_include`, cambio
de encabezado transitivo, diagnóstico ante encabezado transitivo inválido y
reutilización tras restaurarlo. Con `__TIME__` observó la caché deshabilitada.
Ambas ejecuciones terminaron con código 0; la construcción fue incremental.

Estas muestras no certifican todas las formas C ni habilitan la caché de C++.
La batería integral y los demás pendientes de la tanda 8 continúan abiertos.
