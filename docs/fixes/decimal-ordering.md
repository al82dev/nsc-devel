# Comparaciones decimales de orden

El control independiente mostró que `-0.1m > -1.0m`, `0.10m <= 0.1m` y `0.1m >= 0.10m` devolvían falso en O0 y O2. Los controles por caso, comandos y configuración están en `A:/nsc-fixes-agent1-temp/decimal-order-observe-20261112.log`; el compilador utilizado tenía otros cambios locales.

La ruta de orden decimal comparaba los bits empaquetados como entero con signo. La corrección se limita a `<`, `<=`, `>` y `>=` de decimal: alinea coeficientes de 96 bits a 320 bits según la escala, trata ambos signos de cero como cero y compara magnitud con inversión para negativos. Las ramas de igualdad y de enteros permanecen intactas. La anchura de 320 bits alcanza incluso la diferencia codificada de escala de 63; esto es razonamiento sobre representación interna, no una promesa de que el parser admita literales de esa escala.

Tras build incremental de la copia principal, `decimal_ordering` y la regresión preexistente de igualdad pasaron en O0/O2; log `A:/nsc-fixes-agent1-temp/decimal-order-integrated-20261112.log`. El mismo sondeo inicial repitió sus 18 combinaciones en O0/O2 con resultados esperados; log `A:/nsc-fixes-agent1-temp/decimal-order-after-20261112.log`. El fixture público incluye escala 28, negativos, cero, un coeficiente mayor de 64 bits producido por multiplicación y controles de igualdad y enteros. No demuestra por sí solo que el cero fuente negativo conserve su bit de signo ni cubre el máximo de 96 bits. No se ha inspeccionado exhaustivamente la ausencia de helpers externos. La evidencia procede de un árbol con WIP y no certifica una revisión limpia ni todas las operaciones decimales.

El control separado `decimal_ordering_bits` fabricó la representación mediante `unsafe` y pasó en O0/O2 con el mismo compilador: verificó el bit del cero negativo y comparó coeficiente máximo de 96 bits, escala codificada 63 y signos; log `A:/nsc-fixes-agent1-temp/decimal-order-bits-20261112.log`. Este caso prueba el carrier interno, no que esos valores o escala sean literales admitidos por el lenguaje.

Las pruebas posteriores usaron el compilador SHA-256 `139293beee479b780a99ce66a3f5d7391108c71c08105f82da5775de6972d597`; los logs registran los hashes del runtime e import libraries antes y después. Las 18 combinaciones consisten en nueve casos independientes ejecutados en ambos niveles, no en 18 escenarios distintos.

Los dos fixtures exactos incorporados se repitieron desde la rama principal:
cuatro compilaciones y cuatro ejecuciones en O0/O2 devolvieron 0. Registro
`A:/nsc-fixes-agent1-temp/decimal-order-both-integrated-20261112.log`.
No se ejecutaron mediante CTest; se mantienen las limitaciones anteriores.
