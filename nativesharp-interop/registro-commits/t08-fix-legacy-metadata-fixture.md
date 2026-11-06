# Tanda 08: fijar la versión del fixture NSMETA 1.41

La suite detectó dos fallos porque el fixture que comprueba 1.41 tomaba la
versión actual por defecto, que ahora es 1.42. Se fija explícitamente 1.41 para
seguir probando el contrato histórico y el rechazo de catálogos C en él. La
corrección requiere reconstrucción y repetición de la suite.
