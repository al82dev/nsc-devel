# Dirección de campo C con receptor readonly

El campo importado de un receptor readonly deja de ser direccionable como
puntero mutable. La asignación al campo ya consultaba el mismo estado; faltaba
la protección de la dirección. Esto prepara la lectura de structs C globales
const y requiere pruebas de escritura y dirección en la batería agrupada.
