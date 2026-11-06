# Requisito de TLS de biblioteca C

Se prepara un proyecto mixto que llama una función de una biblioteca C con
almacenamiento TLS estático. La función comprueba valor inicial e independencia
entre dos hilos usando Win32. El fixture aún no se ha ejecutado y no habilita
la importación directa del global; permitirá localizar si enlace y arranque
actuales soportan el requisito sin modificar NSLink.
