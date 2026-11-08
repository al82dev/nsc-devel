# T08: restauración del encabezado tras fallo en subrutina TLS

Un fallo inducido dentro del consumidor DLL mostró que el script volvía a
devolver cero tras restaurar el encabezado. Se traslada la restauración a un
destino de fallo explícito que conserva la señal de error. Pendiente de repetir
el fallo inducido y la ruta normal.
