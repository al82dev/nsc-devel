# Sesión de prueba con USR de campo alterado

El adaptador de prueba delega a una sesión Clang real y altera únicamente el
USR del record referenciado por el campo `Branch.items` en el snapshot. Permite
comprobar el rechazo del cierre de dependencias sin modificar Clang ni los
archivos del usuario. Aún falta incorporar el caso y validarlo agrupadamente.
