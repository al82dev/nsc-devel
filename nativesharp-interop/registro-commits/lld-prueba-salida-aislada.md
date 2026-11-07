# Aislamiento entre ejecuciones de la prueba de LLD

La segunda ejecución del fixture reutilizó una carpeta de salida que ya
contenía otra versión de `api.lib`; NSC rechazó correctamente sustituirla.
El fixture ahora usa una carpeta nueva bajo A: para cada ejecución, sin
borrar las salidas anteriores. Los dos enlaces incrementales de una misma
ejecución siguen compartiendo esa carpeta. La prueba conserva todos sus
controles; su resultado después de este ajuste queda pendiente.
