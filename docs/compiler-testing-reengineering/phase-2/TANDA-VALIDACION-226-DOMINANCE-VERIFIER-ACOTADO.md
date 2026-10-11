# Tanda de validación 226 — verificador Dominance acotado

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE PRODUCTO PENDIENTE.

## Causa confirmada

`DominanceVerifier` materializaba un `unordered_set` de dominadores por cada
nodo alcanzable y después recorría matrices cartesianas para consultas,
children y fronteras. El costo de memoria era cuadrático y varias consultas
añadían trabajo cúbico en CFG lineales profundos. La causa era compartida por
`TF-DOM-024`, `TF-DOM-032`, `TF-DOM-040`, `TF-DOM-104`, `TF-DOM-128`,
`TF-DOM-216`, `TF-DOM-224`, `TF-DOM-232` y `TF-DOM-256`.

## Corrección

- El oráculo independiente conserva un dominador inmediato esperado por nodo y
  converge iterativamente sobre RPO.
- Children se invierten en una sola pasada y se validan junto con nesting y no
  solapamiento de intervalos.
- Las matrices exhaustivas de consultas se conservan para grafos de hasta 64
  nodos; a escala se prueban invariantes estructurales y una cadena extrema.
- La frontera esperada se construye desde cada join ascendiendo por idoms, sin
  barrer todos los pares owner/candidate.
- Todos los recorridos son iterativos y las cadenas quedan acotadas por `V`.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
su corrida posterior; este documento no promueve ningún caso a `PASS`.
