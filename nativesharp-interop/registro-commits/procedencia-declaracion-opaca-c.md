# Procedencia de declaración adelantada C

La captura de un record `ForwardOnly` descartaba la procedencia disponible en
las observaciones de Clang. Se prepara su conservación solo cuando todas las
observaciones coinciden; cualquier desacuerdo elimina la evidencia. No se
añade tamaño ni se concilian identidades en este paso. El codec y la comparación
deben actualizarse antes de producir metadata con esta procedencia.
