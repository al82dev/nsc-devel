# Tanda 08: diagnostico de layout de enum

La primera suite fallo el layout del struct con enum sin informar tamaño ni
diagnosticos. La prueba ahora muestra esos datos al fallar para distinguir un
error de construccion de un resultado de layout incorrecto. Falta repetirla.
