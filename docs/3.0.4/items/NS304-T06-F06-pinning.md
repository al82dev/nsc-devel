# NS304-T06-F06 — Pinning y construcción in-place para frames autoreferenciales

- **Estado**: AUDITADO (bloqueado por F05 surface + T13 in-place no congelado)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Integrar pinning y construcción in-place para frames autoreferenciales*.
Plan T06-F06 (DeepSeekDocs): *frames autoreferenciales se construyen en ubicación final antes de crear
referencias internas; mover un frame pinned es error; conectar con contratos de in-place existentes,
sin adelantarse a cambios de T13 no congelados*.

Estado real:

- **Frame actual**: el MoveNext aloca el frame con `entryBlockAlloca` (stack del caller) y lo guarda
  en `frame@8`. Un alloca de stack **no se mueve**, así que el "pinning" está trivialmente satisfecho
  para el único storage existente (caller-stack).
- **Frame autoreferencial**: sin surface. Requiere items `ref`/`ref mut` (F05, decisión de surface
  pendiente) para que un local del frame preste a otro. Hoy el item es siempre por valor (copia).
- **Construcción in-place**: no hay ruta productiva; los contratos de in-place pertenecen a T13
  (allocators/scope/inplace), que el plan F06 dice explícitamente NO adelantar.

## Gap

No hay un frame autoreferencial demostrable (depende de F05), y la construcción in-place/pinning
para storage movible (arena/heap) depende de T13 no congelado. Con el storage actual (stack) el
invariante "el frame no se mueve" ya se cumple por construcción.

## Dependencias

- **F05** (modo `ref`/`ref mut` del item) es prerrequisito para un frame autoreferencial.
- **T13** (in-place/allocators) es el contrato de storage movible; F06 no debe adelantarse.

## Nota

F06 queda **bloqueado** hasta: (1) la decisión de surface de F05 (`Iterator<ref T>`), y (2) el
contrato in-place de T13. Con caller-stack el pinning es un no-op correcto. No hay hueco productivo
que implementar sin violar "no adelantarse a T13".
