# Callback C almacenado con enum en la firma

El fixture prepara un callback que recibe y devuelve `enum Mood`, almacenado
en un campo y un global C. NativeSharp lo lee e invoca y después asigna un
callback sin capturas al almacenamiento para que C lo invoque. La prueba no
usa operaciones mixtas enum/entero, pendientes de decisión por separado.
Todavía no se ha reproducido ni validado este caso.
