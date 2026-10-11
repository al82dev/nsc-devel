# Tanda de validación 223 — metadata corrupta de Dominance

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN CONJUNTA PENDIENTE.

## SCOPE LOCK

La tanda corrige exclusivamente el fixture `TF-DOM-030`. El producto y los
demás casos permanecen protegidos.

## Causa y corrección

La variante inválida volvía a agregar un hijo que `addChild` ya conservaba, por
lo que no alteraba el árbol y el verificador aceptaba correctamente el modelo.
El fixture registra ahora la raíz como su propio hijo: una relación imposible
que contradice el idom esperado y obliga al rechazo estructural previsto.

## Validación

No se compiló ni ejecutó por orden del usuario. Se validará con el conjunto
Debug/O0 al terminar la implementación de todas las causas.
