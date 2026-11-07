# T08-015: controles de promociones enteras restantes

El fixture prepara operaciones con bool, enteros con y sin signo de 8 y 16
bits y desplazamientos con contadores `uint`, `long` y `ulong`. Se incluyen
valores negativos y comprobaciones que distinguen operar en i32 de operar en
i1. El verificador IR comprueba accesos y promociones i32/i64; el control
negativo de bool nativo se conserva.

Estos controles están preparados, sin construir ni ejecutar. El ítem y la
tanda siguen abiertos y no se modificó NSLink.
