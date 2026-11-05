# Tanda 05: adaptador de diagnósticos

El frontend convierte la severidad y conserva mensajes y notas de Clang. Para
una ubicación externa sin correspondencia segura conserva archivo, línea y
columna en el texto; un resolvedor opcional puede proporcionar un rango de NSC
cuando haya verificado que ambas fuentes son idénticas. Los errores fatales se
representan como errores de NSC, con la indicación `fatal` conservada.
La validación se ejecutará agrupada al terminar la tanda.
