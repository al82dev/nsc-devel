# Diagnóstico de asignación `void` en el parser

`d95455f1b` (2026-10-12) rechaza `void` como elemento de asignación en el parser, con
"void cannot be used as an allocation element type". Ese error detiene el análisis, así que el
diagnóstico semántico que explica la regla ("stackalloc requires an element type with a size; void
has none") ya no se alcanzaba. La comprobación cc1 `cc1-stackalloc-void-element`, que exige que el
mensaje diga por qué, fallaba desde entonces; `stackalloc_void_pointer.py` exige el texto del parser.

El parser conserva su diagnóstico y su identificador (`InvalidType`, que es lo que verifican
TP-PAR-069 y TP-PAR-476) y añade la razón con el nombre de la construcción:
"void cannot be used as an allocation element type; stackalloc requires an element type with a
size". `parseAllocationElementType` recibe la palabra (`new` por omisión, `stackalloc` desde la
expresión `stackalloc`).

Resultado: `nsc.cc1.correctness --rest` (494 comprobaciones) y `nsc.compiler.stackalloc-void-pointer`
pasan, sin modificar ninguna de las dos pruebas.
