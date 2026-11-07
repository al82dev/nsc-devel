# Selección del enlazador en ejecutables de NSC

El generador de ejecutables acepta una selección por instancia y usa el
resolutor explícito para generar import libraries y realizar el enlace. La
misma selección se utiliza para identificar la herramienta al construir la
clave de enlace incremental. Una selección vacía conserva el descubrimiento
anterior. No se cambia NSLink ni el entorno global.

Esta pieza aún no recibe la selección del comando público y no cubre los
generadores de bibliotecas. Su construcción y validación están pendientes
del conjunto de propagación de la tanda.
