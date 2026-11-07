# T08-004: seleccionar globales C internas por contrato

Los cinco nombres reproducidos fallaban antes de alcanzar la proyeccion ya
existente. Se elimina la lista local de formas y se dejan pasar las globales C
internas al mismo control de tipos y calificadores que usa el importador; el
diagnostico de multiples origenes se conserva. Las variantes no representables
deben continuar fallando en la proyeccion. Construccion y pruebas pendientes.
