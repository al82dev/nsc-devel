# NativeSharp 2.0 RC freeze

`2.0.0-rc.1` congela la superficie del lenguaje, compilador, formatos, ABIs, runtime, linker,
paquete y documentación que se propone promover como `2.0.0`.

No se admiten características ni cambios de contrato después del congelamiento. Una corrección de
bloqueo obliga a emitir otra candidata y repetir la certificación. RC.1 fija NSMETA 1.20, Native
ABI 1.0, Runtime ABI 1.0, runtime symbol set 2 y MIR 1.11, junto con `NSBREAKING 1`,
`NSMIGRATION-GUIDE 1` y `NSCOMPAT-MATRIX 1`.

La candidata no es la promoción final. Ésta requiere la certificación N-1/N y el baseline completo
de la tanda 16, seguidos por una decisión explícita de promoción.

---

# NativeSharp 2.0 RC freeze

`2.0.0-rc.1` freezes the language, compiler, format, ABI, runtime, linker, package, and documentation
surface proposed for promotion as `2.0.0`. No feature or contract change is allowed after the
freeze. A release-blocking fix requires a new candidate and complete recertification. The
candidate is not the final release; final N-1/N certification, the complete batch baseline, and an
explicit promotion decision remain mandatory.
