# T08: control de operaciones compuestas en `_Bool` C escalar

El fixture prepara la misma matriz de operaciones y operandos previamente
probada sobre el arreglo, ahora usando el global C escalar. El programa
comprueba el valor final también desde C; el validador prepara O0–O3 y
comprueba el IR de byte y las promociones mediante el verificador existente.
La generación verifica la forma esperada del fixture fuente y escribe solo en
el directorio de build; el bool nativo mantiene su control negativo.

Está preparado sin construir ni ejecutar, por lo que no acredita soporte ni
cierra la tanda. No se modificó NSLink.
