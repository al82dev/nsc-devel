# Callback C en campo y global

Se preparó un consumidor que lee e invoca un puntero a función almacenado en
un campo de struct C y otro en un global C. La prueba se agregó a la matriz
O0–O3. La primera ejecución llegó al caso O0 y NSC rechazó ambos accesos:
el campo `run` no se proyectó y el global `shared_callback` tiene tipo no
mapeado. El script no alcanzó sus marcadores finales; el código de salida
observado del proceso lanzador fue 0, por lo que no basta usarlo para declarar
éxito. Se requiere corregir la proyección y verificar todos los marcadores.
