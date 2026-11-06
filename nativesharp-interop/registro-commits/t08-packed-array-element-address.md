# T08-016: dirección de elementos en campo de arreglo importado

El análisis de índices no transmitía la restricción de alineación del campo al
elemento, por lo que permitir arreglos empaquetados dejaría escapar un puntero
ordinario a un elemento subalineado. Para arreglos fijos de campos importados
se compara la alineación efectiva del campo con la natural del elemento
escalar o puntero y se impide exponer la dirección cuando es insuficiente.
Las lecturas y escrituras directas conservan su ruta. Los arreglos empaquetados
siguen rechazados por el proyector hasta completar y probar el circuito.
