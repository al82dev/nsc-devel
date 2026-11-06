# Origen C y alineación del arreglo volatile

El proveedor de globales registra si todas las declaraciones reconciliadas
provienen de C. La dirección HIR de un arreglo global C volatile de int con
modo MS y alineación declarada suficiente conserva una garantía conservadora
de cuatro bytes. No se aplica a declaraciones C++ ni a alineación desconocida.
La emisión de orden por elemento y su validación agrupada quedan pendientes.
