# NS3 baseline: caso positivo genérico completo en NSMETA 1.38

## Problema

La prueba de compatibilidad Stage 50.9 convertía una plantilla pública incompleta en caso positivo
añadiendo únicamente su `sourceText`. Desde NSMETA 1.38, cada parámetro genérico también requiere un
contrato posicional; `Box<T>` seguía siendo un modelo inválido porque tenía aridad uno y cero
contratos.

## Cambio

El caso positivo añade el contrato de parámetro de tipo `T` (`kind = 0`) antes de pedir la
verificación actual. El caso negativo previo conserva su propósito y continúa fallando primero por
la ausencia de una declaración materializable.

## Archivos

- `tests/unit/Stage50_9MetadataHostCompatibilityTests.cpp`

## Mejora

Stage 50.9 construye un modelo actual completo y mide compatibilidad real con NSMETA 1.38, sin
debilitar la validación de aridad, ownership, constraints o lifetimes del codec.
