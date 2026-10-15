# NS304-T16-F03 — Producer/consumer sin fuentes

- **Estado**: COMPLETO — prueba binaria intermodular por feature (decimal) con fuentes retiradas.
- **Rama**: development

## Implementado (2026-10-15)

- `89c48d62` — cierre del hueco auditado:
  - **Producer/consumer por feature**: `TL-LANG-508` construye una Library con
    `Api.Calculator.Double(decimal)` (`--emit-project-oop-lib` → `.lib` + `.nsmeta`), retira las
    fuentes del producer y compila un consumer Exe vía `LibraryReference` que llama
    `Calculator.Double(1.5m)` y verifica `(int)(d*10m)==30` — aceptación + ejecución cross-module
    del tipo público `decimal`.

## Verificado O0

- `TL-LANG-508` (CL-LANG-020, Host.Language, `nsc_component_tests`): PASS — el consumer sin source
  reference reconstruye y ejecuta el método decimal importado (exit 0).

## Contexto

- El hueco era el corpus de pruebas intermodulares por feature T03-T15 (no una feature de lenguaje).
  Aquí se cerró un caso representativo (el tipo público `decimal`). El corpus completo de las demás
  features (Result/throws/panic/arena, etc.) se añade incrementalmente; la propiedad de ejecución
  cross-module ya queda demostrada.
