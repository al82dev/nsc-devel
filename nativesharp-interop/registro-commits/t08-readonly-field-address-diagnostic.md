# Diagnóstico de dirección de campo readonly

El rechazo de la dirección de un campo de struct const mencionaba bit fields
sin que existiera uno. El análisis informa ahora que el almacenamiento es
readonly cuando esa es la razón, conservando los demás diagnósticos. Requiere
repetir los fixtures const junto con la batería de la tanda.
