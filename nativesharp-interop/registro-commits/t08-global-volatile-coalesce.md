# T08-015: orden de `??=` sobre global C volatile

El análisis acepta `??=` para almacenamiento puntero. El emisor de esta
operación marcaba volatile sus accesos, pero no consultaba la comprobación
existente de modo MS, tipo y alineación que sí se usa en asignaciones simples
y compuestas. Se aplica esa misma comprobación a la lectura y a la escritura
condicional; la evaluación del fallback y los demás destinos no cambian.
La prueba de integración y su ejecución agrupada permanecen pendientes.
