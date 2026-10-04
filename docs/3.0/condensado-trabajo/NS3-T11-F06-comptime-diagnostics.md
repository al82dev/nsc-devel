# NS3-T11-F06 — Diagnósticos comptime / Compile-time diagnostics

## ES

`#compile_error`, `#compile_warning` y `#compile_note` aceptan mensaje y código de paquete
constantes; opcionalmente aceptan offset y longitud del span autorizado. El motor comptime borra la
directiva antes del lexer, conserva el registro y `ProjectCompilation` lo publica mediante el
`DiagnosticEngine` ordinario. Los spans se comprueban contra el archivo, los errores detienen la
compilación y warnings/notes no la invalidan.

Los códigos de paquetes deben tener namespace propio y no pueden comenzar por `NS3`, reservado al
compilador. Un diagnóstico producido dentro de una expansión incluye notes deterministas con el
backtrace de macros. El mensaje literal funciona como fallback de localización; herramientas pueden
traducir por código sin cambiar la salida reproducible del compilador.

## EN

`#compile_error`, `#compile_warning`, and `#compile_note` accept constant message and package-code
strings plus an optional authorized span offset and length. The comptime engine blanks the directive
before lexing, retains its record, and `ProjectCompilation` publishes it through the ordinary
`DiagnosticEngine`. Spans are bounds-checked; errors stop compilation while warnings and notes do not.

Package codes require their own namespace and cannot start with compiler-reserved `NS3`. Diagnostics
created in expansions carry deterministic macro-backtrace notes. The literal message is the locale
fallback; tools may translate by code without changing reproducible compiler output.

## Evidencia / Evidence

`nsc_ns3_t11_f06_comptime_diagnostics_tests` covers severities, package codes, macro backtraces,
valid source ranges, reserved-code collisions, and out-of-bounds span rejection.
