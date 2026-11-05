# Tanda 08: prueba preparada de conflicto diferido de enum

Dos sesiones con definiciones incompatibles del mismo enum deben producir un
candidato con diagnóstico diferido, sin fallar durante la recolección. Esta
aserción no comprueba por sí sola el diagnóstico al usar el tipo. La prueba
todavía no se ha ejecutado y se incluye en la validación agrupada.
