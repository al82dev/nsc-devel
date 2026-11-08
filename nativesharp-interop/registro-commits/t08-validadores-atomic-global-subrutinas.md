# T08: propagación a través de dos subrutinas precompiladas

Un fallo inducido en la DLL mostró que la señal se perdía después de salir del
consumidor y restaurar el encabezado. El script conserva el resultado, restaura
el archivo y dirige el error al destino explícito tanto en la subrutina como
en el bucle exterior. Pendiente de validación.
