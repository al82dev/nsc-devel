# Tanda 08: ejecución de callback con enum C

- El fixture declara un typedef de callback que recibe y devuelve `enum Mood`, y una función C que lo invoca.
- El programa NativeSharp prepara una lambda que devuelve el enum recibido, la pasa a C y comprueba el resultado negativo por medio de la función C existente.
- Estado: prueba preparada, todavía no ejecutada. No presupone que la ruta funcione ni cierra T08-009 o T08-017.
