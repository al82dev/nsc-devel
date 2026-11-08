# T08-003: corrupción de sidecars v3 en integración

Se amplía la matriz de funciones C inline para alterar la huella de uno y luego
de los dos sidecars v3 producidos. Cada consumidor O3 debe ejecutarse, usar una
o cero bibliotecas ThinLTO respectivamente y registrar el fallback del sidecar
dañado. Los sidecars originales se guardan y restauran en la carpeta temporal.
El resultado se registrará tras ejecutar el conjunto.
