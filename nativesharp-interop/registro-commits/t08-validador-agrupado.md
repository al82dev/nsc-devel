# Tanda 08: ejecucion agrupada preparada

El script construye incrementalmente NSC con interop, la suite de sesiones y
el frontend sin interop; luego ejecuta la suite y los siete proyectos preparados,
sin detenerse por el primer caso fallido. No se ha ejecutado. Un resultado
exitoso requiere ademas auditar cobertura y evidencia antes de cerrar items.
No utiliza ctest, cuyo entorno anterior apuntaba al compilador de C:.
