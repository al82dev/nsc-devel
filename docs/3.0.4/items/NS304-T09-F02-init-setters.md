# NS304-T09-F02 — Setters `init` y congelamiento posterior a construcción

- **Estado**: CERRADO (parser + marcador `:init` + restricción en constructor)
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

## Cierre

- **Congelamiento**: efectivo por la combinación de la restricción `init` (solo en constructor) y
  el backing field privado (`__nsc_backing_<name>$stage38`); tras la construcción no hay vía de
  escritura externa.
- **Object initializer con property assignment** (`new T { X = 1 }`): el initializer de agregado
  actual inicializa **campos**, no propiedades; habilitar setters de property en `new T { ... }` es
  una capacidad aparte (fuera del alcance de T09-F02).
