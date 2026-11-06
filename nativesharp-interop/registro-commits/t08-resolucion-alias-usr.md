# Resolución de aliases con USR

T08-002/T08-005: la resolución de aliases de record y enum comprueba los USR
cuando ambos son conocidos, tanto en aliases directos como en arreglos y firmas.
Se conservan las condiciones existentes de nombre, propietario y tipo, y la
ruta previa si falta un USR. Las pruebas y la validación están pendientes.
