# 386 — Límites y aislamiento de comptime / Compile-time execution sandbox

La ejecución de `#run` es hermética por defecto. El evaluador no resuelve identificadores, no
llama funciones, no carga FFI y no ofrece acceso implícito a archivos, variables de ambiente,
reloj, red, procesos ni aleatoriedad. Un intento se rechaza como error de sandbox antes de que el
host pueda observar un efecto. / `#run` execution is hermetic by default. The evaluator resolves no
identifiers, calls no functions, loads no FFI, and offers no implicit access to files, environment,
clock, network, processes, or randomness. An attempt is rejected as a sandbox error before the host
can observe an effect.

Cada ejecución tiene límites explícitos de bytes de fuente, cantidad de directivas, pasos y
anidamiento. Los defaults son 64 MiB, 1024 directivas, 100 000 pasos y 128 niveles. Rebasarlos
produce un diagnóstico determinista, no un hang ni un crash del compilador. / Every execution has
explicit source-byte, directive-count, step, and nesting limits. Defaults are 64 MiB, 1024
directives, 100,000 steps, and 128 levels. Exceeding one produces a deterministic diagnostic rather
than hanging or crashing the compiler.
