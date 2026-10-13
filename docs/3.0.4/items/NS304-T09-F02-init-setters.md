# NS304-T09-F02 — Setters `init` y congelamiento posterior a construcción

- **Estado**: PARCIAL (parser + restricción en constructor) / residual (object initializer + congelamiento)
- **Rama**: development

## Resultado

- **Parser** (`src/Parser/Parser.cpp`): `init` se reconoce como keyword contextual en property e
  indexer, baja como `set_<name>` y marca el accessor con el sufijo `:init` en el metadata
  (`property:set:Value:init`), sin pisar el prefijo que reconoce getter/setter.
- **Member model** (`ProjectMemberModel.h/.cpp`): nuevo flag `ClassMemberSymbol::isInit` derivado
  del sufijo `:init`.
- **Sema** (`analyzeAssignment`): un setter `init` fuera de un constructor se rechaza con
  diagnóstico claro ("init accessor can only be assigned during construction").
- Prueba `TL-LANG-334`: positivo (asignación en constructor → 42) y negativo (asignación en `Main`
  rechazada).

## Residual

- **Object initializer** (`new T { X = 1 }`): debe permitir `init`, pero requiere rastrear el
  contexto de inicialización en Sema (hoy solo se reconoce el contexto de constructor).
- **Congelamiento posterior a construcción**: marcar el backing field de solo lectura al terminar la
  construcción.

