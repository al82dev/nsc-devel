# Control de importación por nombre empaquetado

El control anterior mostró que importar `api.dll` y cargar una copia como `alias.dll` crea dos módulos. Para comprobar otra forma de resolver la importación directa sin modificar NSLink, este control genera una biblioteca de importación cuyo nombre de DLL es el nombre empaquetado y enlaza contra ella un ejecutable C de referencia.

`diagnosticar-import-alias.py` reutiliza el control PE/TLS existente, genera el archivo de definición y la biblioteca de importación con `llvm-dlltool`, verifica la tabla de importaciones con `llvm-readobj` y copia el ejecutable junto con la DLL a otro directorio. Ejecutado desde `A:\`, el control comprobó que la dirección exportada y la función importada pertenecen al mismo módulo, que leen y escriben el mismo estado TLS en dos hilos y que el estado de cada hilo permanece independiente. Resultado: código 0.

Esto valida el mecanismo en C y en la máquina actual. No implementa todavía la selección de importaciones ni su empaquetado en NSC, no prueba dependencias transitivas ni acredita el consumidor NativeSharp. Esas tareas permanecen abiertas en el tracking.
