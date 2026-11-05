# Destrucción C++ durante limpieza

La limpieza busca el destructor externo del tipo importado. Si un destructor
que puede lanzar se ejecuta durante el desenrollado, el código genera una
frontera que termina el proceso si el destructor vuelve a lanzar.

Esta implementación depende del ABI y runtime Windows seleccionados y requiere
la validación conjunta de los casos de excepción y vida útil.
