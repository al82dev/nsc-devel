# Regresión de construcción fallida con bases

Se registra la prueba dirigida de construcción fallida mediante Rc y Arc. El
fixture distingue fallo en la base y después de su construcción, verifica el
orden de destrucción y cubre propagación y captura de excepciones. La prueba
usa definiciones mínimas de Send y Sync cuando compila con fuentes y las del
SDK al consumir la biblioteca binaria; desactiva la caché de artefactos.

El 11 de noviembre de 2026 pasó con las fuentes de Core para ambas familias
en O0 y O2, y pasó también con el Core binario aislado preexistente. Comandos y
salidas: `A:/nsc-fixes-evidencia/shared-throwing-base-after-integration-20261111.cmd`
y los tres logs `shared-throwing-base-*-after-20261111.log` de esa carpeta.
El compilador aislado contenía otros cambios locales y el Core binario no se
regeneró en esta repetición; el resultado no certifica este commit limpio, todas
las construcciones parciales ni una segunda excepción durante la limpieza.
