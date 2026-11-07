# Controles de calificadores en arreglos TLS

El fixture ampliado comprueba que el IR estático conserva carga y escritura
`volatile` y que NSC rechaza escribir el arreglo `const` tanto en estático
como en DLL. Se ejecutan después de los casos positivos O0–O3 y no sustituyen
la validación completa. La primera versión del guion tuvo un error de sintaxis
CMD al escribir código entre paréntesis; se sustituyó por una copia de fuente.
La repetición pasó ambos controles y los casos O0–O3 con NSC
`df824a793de82e55`.
