# Callback C en campo y global

Se preparó un consumidor que lee e invoca un puntero a función almacenado en
un campo de struct C y otro en un global C. La prueba se agregó a la matriz
O0–O3. La primera ejecución llegó al caso O0 y NSC rechazó ambos accesos:
el campo `run` no se proyectó y el global `shared_callback` tiene tipo no
mapeado. El script no alcanzó sus marcadores finales; el código de salida
observado del proceso lanzador fue 0, por lo que no basta usarlo para declarar
éxito. Tras conectar el mapeo y corregir el layout, el script mostró los ocho
marcadores de ejecución: callback retornado y callback en campo/global en
cada nivel O0–O3. Los casos usan `int(int)`; siguen fuera de la evidencia
otras firmas, calificadores y arquitecturas.
