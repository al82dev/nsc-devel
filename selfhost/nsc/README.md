# nsc-next

Este árbol contiene el compilador escrito en NativeSharp. Toda fuente y proyecto pasa el contrato
`NativeSharp.BootstrapProfile/2.0` antes de que `nsc0` lo compile. El primer ejecutable, cerrado por
SH04, vive en `NativeSharp.Compiler.nsproj` y separa físicamente:

- `Basic`: IDs tipados, resultados, hash estable y arena con handles generacionales;
- `Source`: buffers propietarios, normalización LF, posiciones, rangos y line maps;
- `Diagnostics`: catálogo estable, render humano compatible y JSON versionado;
- `Driver`: identidad, ayuda, códigos de salida y lectura de fuente.

SH05 añadió Lexer, Syntax y Parser. SH06 añadió Symbols, Types, Sema, layout Win64 y el primer
contrato de Metadata. SH07 añade `IR`: HIR, MIR explícito, CFG, dominancia, SSA, verificadores y
perfiles de pases O0/O1/O2/O3. Las fases siguientes añadirán CodeGen.LLVM, Projects y Cache sin
convertir este proyecto en un conjunto de referencias circulares. Este árbol no puede
contener wrappers C++ ni una segunda implementación oculta del frontend.
