# Alcance de arreglo global C volatile _Bool

El valor NS bool se representa como i1; Clang accede al almacenamiento C
como i8 atómico. Se prepara una conversión explícita limitada al global C con
modo MS. La asignación simple y lectura son la primera parte; otras
operaciones quedan abiertas y deben tratarse antes de cerrar la tanda.
