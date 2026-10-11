# Tanda de validación 224 — cadena Dominance a escala

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN CONJUNTA PENDIENTE.

## Causa y corrección

`TF-DOM-072` reconstruía un oráculo de conjuntos y una cadena completa por cada
nodo del grafo lineal. Esa combinación convertía una prueba de escala en trabajo
superlineal redundante. La variante NIGHTLY consulta ahora una sola cadena
extremo a extremo y verifica tamaño, raíz y cola; las variantes pequeñas
conservan la comparación independiente nodo por nodo.

## Validación

No se compiló ni ejecutó por orden del usuario. Se validará al final del barrido
con el conjunto Debug/O0.
