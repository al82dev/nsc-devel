# T08: control de caché para bool C precompilado

El fixture prepara compilaciones repetidas del productor y consumidor con
caché de artefactos aislada, comprueba el indicador de hit y ejecuta el
consumidor con el header C oculto en O0–O3. Restaura el header después de cada
intento. Está preparado sin ejecutar y no acredita el comportamiento todavía.
