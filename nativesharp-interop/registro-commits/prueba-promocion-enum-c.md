# Comparación de enum C con enumerador entero

Se conserva como caso independiente la comparación `fromHeader != CGlobalChoice`.
En C el enumerador es entero y el valor enum participa en las conversiones
aritméticas; NSC la rechazó como tipos incompatibles en O0. La prueba sigue en
la batería y aún falta implementar y validar esa conversión.
