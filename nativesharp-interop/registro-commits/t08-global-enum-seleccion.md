# Tanda 08: selección inicial de enum por global demandado

- La observación previa identifica globales externos de tipo enum, captura demandas y selecciona candidatos enum del proyecto visible y namespace correspondiente. Los conflictos de candidatos se conservan como fallo; no se seleccionan globales sin demanda.
- La proyección posterior comprueba el índice nominal y el layout mediante `projectCppSemanticEnumType`. El fixture `hidden_global` comprueba el caso donde el tipo no se usa en ninguna firma consumida.
- Estado: implementación y fixture pendientes de validación agrupada. Punteros a enum, sombras locales y demás combinaciones requieren comprobación específica; T08-004 y T08-009 siguen abiertos.
