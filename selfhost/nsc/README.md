# nsc-next

Aquí vivirá el compilador escrito en NativeSharp. Los módulos se añadirán únicamente después de
cerrar SH00 y SH01. Toda fuente y proyecto añadido a este árbol debe pasar el contrato
`NativeSharp.BootstrapProfile/2.0` antes de que `nsc0` lo compile. La arquitectura prevista separa Basic, Source, Diagnostics, Lexer, Syntax,
Parser, Symbols, Types, Sema, HIR, MIR, CFG, SSA, CodeGen.LLVM, Target.Win64, Metadata, Projects,
Driver y Cache.

Este árbol no puede contener wrappers C++ ni una segunda implementación oculta del frontend.
