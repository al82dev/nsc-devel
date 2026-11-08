# T08: reproducción de función C static inline en plantilla precompilada

El productor incluye `fixture.h`, que define `fixture_twice` como `static inline`, y publica una plantilla que llama a esa función. El consumidor recibe solamente la biblioteca y NSMETA y espera obtener 42.

En O0, con el compilador de desarrollo de build `bb39af5659ae785e`, el productor generó biblioteca y metadata, pero el consumidor terminó con código 1: `unknown identifier 'fixture_twice' in callable/initializer context`. Registro: `A:/nsc-interop-build/tanda08-inline-template.log`. Esta prueba muestra el fallo actual; no forma parte aún del validador general ni acredita resolución.
