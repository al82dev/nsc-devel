# NativeSharp.Core.IO referencia Core como proyecto

`lib/NativeSharp.Core.IO/NativeSharp.Core.IO.nsproj` declaraba
`<LibraryReference Include="NativeSharpCore.lib" Metadata="NativeSharpCore.nsmeta" />` relativa a
su carpeta. En `lib/` la biblioteca se entrega como fuente junto a `NativeSharp.Core` y ese
`NativeSharpCore.lib` no existe, así que la biblioteca y sus consumidores fallaban con
"referenced library does not exist". Las fixtures cc1 c24, c25, c26 y c28 fallaban por esto en O0
y O2.

Es la misma situación que `NativeSharp.Core.Collections.Generic` y `NativeSharp.Core.Threading`
corrigieron el 2026-10-25 (ver su `LANGUAGE-3-MIGRATION.md`): el proyecto vuelve a
`../NativeSharp.Core/NativeSharp.Core.nsproj`. La referencia binaria sigue siendo la forma de
`nativesharp-libraries`, donde `NativeSharpCore.lib` sí se construye al lado.

Resultado: c24 y c28 pasan en O0 y O2. c25 y c26 dejaron de fallar por la referencia y mostraron
los fallos propios que se describen en `fixtures-cc1-reglas-3.0.md`.
