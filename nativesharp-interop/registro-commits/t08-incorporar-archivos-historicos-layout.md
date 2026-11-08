# Tanda 8: archivos históricos del carril de layout

Se conservan en NSC los informes originales de comparación y formas pendientes,
el alcance histórico del carril y el parche de `_Bool` que faltaban en el
árbol principal. Provienen de `interop-t08-claude-layout` y documentan la
revisión `ceac3f83b`; sus afirmaciones de capacidades pendientes describen ese
momento, no necesariamente el comportamiento actual. El parche es evidencia
histórica, no una instrucción para aplicarlo de nuevo: la corrección `_Bool`
ya se incorporó en `fd8ed5c1a` y la de campos volátiles en `942e77598`.

Los fixtures y documentos restantes de ambos carriles se cotejaron por ruta
contra el árbol de NSC. Su presencia no acredita que cada hallazgo esté
resuelto ni sustituye la validación completa de la tanda 8.
