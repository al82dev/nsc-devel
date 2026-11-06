# Tanda 08: proveedores combinados de miembros C

Prepara la combinación transaccional de campos de sesiones y de metadata, con
rechazo de duplicados. Los enumeradores precompilados se seleccionan solo ante
una demanda que incluya su proyecto y namespace; las colisiones con valores de
sesión se registran como error diferido, siguiendo el modelo existente de NSC.

Los proveedores aún no están conectados al driver ni ejecutados. La conciliación
de una misma declaración en ambas fuentes sigue pendiente.
