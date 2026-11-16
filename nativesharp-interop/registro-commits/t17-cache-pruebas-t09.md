# Tanda 17, cambio 5b: las pruebas de la tanda 9 con la caché de compilación C++

## Defecto

En la regresión agrupada de la tanda 17, 211 guiones C++ de la tanda 9 fallan con `resultado de cache
inesperado`. Cada uno compila su proyecto dos veces con `--artifact-cache` y exige
`compilation-bundle-cache: disabled` en las dos: la aserción describía la barrera anterior al
[cambio 5](t17-cache.md), que dejaba sin caché toda sesión C++.

## Diseño

Política de negativos obsoletos: la aserción pasa a ser una comparación positiva. La primera compilación con
caché guarda la compilación (cualquier motivo salvo `disabled`) y la segunda la reutiliza (`hit`); el
ejecutable de cada una debe dar la misma salida que el programa compilado sin caché, que el guion ya compara
con el control C++ de Clang. El rechazo genuino de una entrada distinta lo comprueba
`pruebas-tanda-17-cache` (diez cambios de entrada frente a Clang).

## Cambio

150 guiones `pruebas-tanda-09-*/validar*.py` con el mismo bloque, convertidos por
`build-claude-t08/tools/convertir-cache-t09.py` (búsqueda exacta del bloque; ninguno quedó sin convertir).
No cambia código de producción.

## Evidencia

Repetición de los guiones que fallaron, con el compilador de la tanda: `logs/regresion-t09-t17r-repeticion.log`
(ver [validación agrupada](../TANDA-17-VALIDACION-AGRUPADA.md)).
