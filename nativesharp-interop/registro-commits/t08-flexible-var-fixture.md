# T08-010: controles de inferencia de miembros flexibles

Los consumidores directo, Shared y solo metadata incorporan lecturas mediante
`var` para un miembro `const` y lectura/escritura para un miembro `volatile`.
Se agregan controles negativos para escritura sobre el primero e inferencia de
un miembro subalineado. Su ejecución y el contraste del IR quedan pendientes.
