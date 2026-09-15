# Ítem 357 — Dot, cross, min, max y clamp / Vector math operations

## Resultado / Result

Los vectores flotantes del núcleo incorporan producto punto 4D, producto cruz 3D y selección por
carril mediante mínimo, máximo y clamp. / Core floating vectors now provide 4D dot product, 3D cross
product, and per-lane selection through min, max, and clamp.

## Pruebas / Tests

Una prueba combinada recorre las cinco operaciones en O0 y O2 y exige resultado 42. / One combined
test exercises all five operations at O0 and O2 and requires result 42.
