# Tanda 08: lectura de puntero global a enum C

- Se prepara un global C que apunta al enum mutable ya probado. NativeSharp lee el puntero, lo desreferencia y pasa el enum a una función C para comprobar el valor.
- Este caso utiliza un enum que también se selecciona por otras firmas; no verifica que el puntero global por sí solo active la selección. Pendiente de la validación agrupada.
