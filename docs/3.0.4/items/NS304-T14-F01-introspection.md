# NS304-T14-F01 — Introspección completa

- **Estado**: COMPLETO — gate `staticKind` efectivo + descriptores tipados (modifiers/ownership/generics/effects/source origin).
- **Rama**: development

## Implementado (2026-10-15)

- `56c26650` — dos cierres del residual:
  1. **Gate efectivo**: `staticKind` (`ProjectMetaprogrammingModel.cpp`) ya no colapsa un
     `ClassMemberKind` desconocido a `Field`; el `default` es ahora `std::abort()`. El switch es
     exhaustivo para los cinco kinds admitidos en v3.0.4 (`Field/Method/Constructor/Destructor/Event`);
     alcanzar el trap es el gate defensivo que pedía la auditoría.
  2. **Descriptores tipados**: `StaticMemberInfo` publica además `isReadonly`, `isConst`,
     `isVolatile`, `declaredOwnership` (`OwnershipKind`), `genericTemplateKey`, `sourcePath` y
     `frameStorage`, poblados desde `ClassMemberSymbol` en `build()`.

## Verificado O0

- `TF-FRONT-505..512` (CF-FRONT-064, Host.Frontend, `nsc_component_tests`): PASS — el catálogo
  estático captura `readonly`/`const`/`volatile`/`owned` de un miembro declarado; variantes de
  fuente vacía, sintaxis inválida, dependencia faltante y determinismo verificadas.
- Unit regresión T11 (`nsc_ns3_t11_f01_typed_introspection_tests`,
  `nsc_ns3_t11_f02_user_attributes_tests`, `nsc_ns3_t11_f03_derive_tests`): PASS — sin regresión
  por la ampliación del struct.

## Contexto

- La introspección es el catálogo estático compile-time-only (`ProjectMetaprogrammingModel`), sin
  descriptor runtime, registro ni símbolo enlazable; la auditoría previa localizó el fallback
  silencioso y dejó pendientes los descriptores tipados + el gate efectivo, ambos cerrados aquí.
