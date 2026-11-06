# T08-010 / T08-012 / T08-020: alineación de campo importado

El modelo de miembros descartaba `byteAlignment` tras validarla; el layout
importado volvía a imponer la alineación natural. El símbolo conserva ahora
el valor importado en el campo ya persistido de alineación y el layout lo
valida y transporta a MIR. La ruta es específica de miembros externos: los
atributos de alineación de campos NativeSharp siguen siendo mínimos.

Esto no habilita la importación de campos empaquetados. El proyector sigue
rechazándolos mientras no se controlen las direcciones escapadas y accesos
indirectos. Quedan pendientes pruebas agrupadas y el resto del contrato.
