# nsc-next

Este árbol contiene el compilador escrito en NativeSharp. Toda fuente y proyecto pasa el contrato
`NativeSharp.BootstrapProfile/2.0` antes de que `nsc0` lo compile. El primer ejecutable, cerrado por
SH04, vive en `NativeSharp.Compiler.nsproj` y separa físicamente:

- `Basic`: IDs tipados, resultados, hash estable y arena con handles generacionales;
- `Source`: buffers propietarios, normalización LF, posiciones, rangos y line maps;
- `Diagnostics`: catálogo estable, render humano compatible y JSON versionado;
- `Driver`: identidad, ayuda, códigos de salida y lectura de fuente.

Las fases siguientes añadirán Lexer, Syntax, Parser, Symbols, Types, Sema, HIR, MIR, CFG, SSA,
CodeGen.LLVM, Target.Win64, Metadata, Projects y Cache sin convertir este proyecto en un conjunto de
referencias circulares. Este árbol no puede contener wrappers C++ ni una segunda implementación
oculta del frontend.
