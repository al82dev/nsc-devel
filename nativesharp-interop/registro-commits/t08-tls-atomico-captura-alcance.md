# Alcance: captura de tipo atómico de Clang

Se delimita la captura del tipo `_Atomic(T)` y su elemento, sin habilitar aún
proyección ni serialización. La implementación completa necesita preservar el
contrato hasta IR y metadata antes de aceptar accesos.
