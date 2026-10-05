# NS3-T14-F06 — Evaluación constante unificada / Unified constant evaluation

## ES

`comptime` reutiliza el mismo evaluador entero y las mismas políticas de overflow que runtime. Un overflow `unchecked` que sería UB se convierte en diagnóstico durante compilación; checked, wrapping y saturating conservan exactamente su contrato. Los modos FP comparten la autoridad de strict/precise/fast y declaran cuándo la paridad bit a bit es exigible.

La memoria constante asigna identidades de provenance no falsificables, limita aritmética y dereference al allocation vivo y detecta use-after-free. Las unions conservan miembro activo. Los loops usan un presupuesto determinista y su resultado puede alimentar const generics sin un intérprete aritmético alterno.

Prueba dirigida: `nsc_ns3_t14_consteval_tests` cubre punteros const, provenance, unions, overflow modes, FP, const generics, loops y paridad runtime/comptime.

## EN

`comptime` reuses the runtime integer evaluator and overflow policies. An `unchecked` overflow that would be UB becomes a compile-time diagnostic; checked, wrapping, and saturating retain their exact contract. FP modes share the strict/precise/fast authority and state when bit-for-bit parity is required.

Constant memory assigns unforgeable provenance identities, confines arithmetic and dereference to a live allocation, and detects use-after-free. Unions retain an active member. Loops consume a deterministic budget, and their result can feed const generics without a second arithmetic interpreter.

Focused test: `nsc_ns3_t14_consteval_tests` covers constant pointers, provenance, unions, overflow modes, FP, const generics, loops, and runtime/comptime parity.
