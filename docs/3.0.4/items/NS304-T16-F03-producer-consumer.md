# NS304-T16-F03 — Producer/consumer sin fuentes

- **Estado**: AUDITADO (proyectos/NSLink existen; prueba producer/consumer por feature sin cerrar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar producer/consumer sin fuentes*.
Plan T16-F03 (DeepSeekDocs): *por cada feature pública construir producer, generar metadata/lib,
retirar sus fuentes de la ruta del consumer y compilar consumer; verificar aceptación, rechazo,
overload, ownership, efectos y ejecución; una referencia accidental al checkout invalida la prueba*.

Estado real:

- Los proyectos + NSLink + el metadata existen; no hay una prueba sistemática producer/consumer por
  cada feature pública T03–T15 (producer → lib → consumer sin fuentes).

## Gap

Falta la prueba producer/consumer sin fuentes por feature pública (aceptación/rechazo/overload/
ownership/effects/ejecución), con las fuentes del producer retiradas de la ruta del consumer.

## Nota

F03 depende de F02 (writer/reader) + el metadata existente. No es una surface nueva; es la prueba
binaria intermodular de cada feature.
