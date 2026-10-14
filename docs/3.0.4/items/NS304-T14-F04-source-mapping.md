# NS304-T14-F04 — Source mapping y macro backtrace

- **Estado**: AUDITADO (spans de fuente existen; mapping generado→invocación/plantilla sin cerrar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar source mapping y macro backtrace*.
Plan T14-F04 (DeepSeekDocs): *toda declaración/token generado mapea a invocación y plantilla; el
diagnóstico muestra causa primaria y cadena de expansión; debugger/navegación vuelve al origen; no
usar spans ficticios globales*.

Estado real:

- Los spans de fuente + diagnóstico primario existen. El mapeo de un token/declaración generado a su
  invocación/plantilla (cadena de expansión, navegación al origen) no está cerrado.

## Gap

Falta el source mapping de generados (invocación + plantilla) + la cadena de expansión en el
diagnóstico + la navegación al origen (sin spans ficticios).

## Nota

F04 depende de F02 (generación tipada, origin) y es el backtrace del macro. No es una surface nueva;
es el mapping de spans generados.
