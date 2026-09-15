# 384 — Certificación humana Visual Studio/WinDbg / Visual Studio and WinDbg certification

## Español

La entrega de depuración extrema queda preparada para una aceptación humana repetible y, en esta
máquina, pasa primero por los mismos consumidores de símbolos que usan los depuradores. El gate
`nsc.debug.consumer-handoff` carga el PDB con `msdia140.dll` del DIA SDK instalado por Visual Studio
2022 y vuelve a cargar el ejecutable con `DbgHelp`, la capa de símbolos de Windows utilizada por el
ecosistema WinDbg. Ambos consumidores deben enumerar las rutas SEH, COM, interfaz y closure.

La aceptación visual se realiza sin recompilar: abrir
`debug-extreme-o0-test/Consumer/DebugExtremeConsumer.exe`, confirmar que el PDB contiguo se carga y
poner breakpoints en `InspectSehStack`, `CaptureComFrames`, `StackCounter.CaptureDepth` e
`InspectClosureStack.lambda#0`. En cada parada, la pila debe atravesar los frames documentados en
los ítems 382–383; los locales de los ítems 377–381 deben conservar sus nombres y tipos. Visual
Studio debe mostrar los métodos autores y los thunks artificiales de forma separada; WinDbg debe
resolver los mismos nombres con `.reload /f`, `x` y `k`.

El gate automatizado no pretende sustituir el juicio visual: evita que una sesión humana empiece
con un PDB que el consumidor real no puede abrir. No se lanza ninguna interfaz gráfica desde CTest,
por lo que la suite es no interactiva y no puede producir cuadros de diálogo.

## English

The extreme-debug delivery is ready for repeatable human acceptance and, on this machine, first
passes through the same symbol consumers used by the debuggers. The
`nsc.debug.consumer-handoff` gate loads the PDB through Visual Studio 2022's installed
`msdia140.dll` DIA SDK and loads the executable again through DbgHelp, Windows' symbol layer used
throughout the WinDbg ecosystem. Both consumers must enumerate the SEH, COM, interface, and closure
paths.

Visual acceptance requires no rebuild: open
`debug-extreme-o0-test/Consumer/DebugExtremeConsumer.exe`, confirm its adjacent PDB loads, and set
breakpoints on `InspectSehStack`, `CaptureComFrames`, `StackCounter.CaptureDepth`, and
`InspectClosureStack.lambda#0`. At every stop the stack must cross the frames documented by items
382–383, while locals from items 377–381 retain their names and types. Visual Studio must separate
authored methods from artificial thunks; WinDbg must resolve the same names with `.reload /f`, `x`,
and `k`.

The automated gate does not claim to replace visual judgment; it prevents a human session from
starting with a PDB the real consumer cannot open. CTest launches no graphical interface, so the
suite remains non-interactive and cannot display message boxes.
