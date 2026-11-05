# Tanda 08: proveedor de typedef en ProjectCompilation

Se incorpora un proveedor opcional y el almacenamiento de sus observaciones al
ciclo de vida de `ProjectCompilation`. Su configuración se rechaza después del
inicio de compilación, siguiendo el contrato de los demás proveedores. Este
cambio no habilita todavía la importación ni instala el proveedor en el driver;
la ejecución y la validación siguen pendientes dentro de la tanda.
