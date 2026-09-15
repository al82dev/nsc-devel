# Ítem 336 — Selección por arquitectura y versión / Architecture and version selection

## Resultado / Result

Los manifiestos `.nssdk` pueden declarar variantes `Target` con arquitectura y rango inclusivo de
versiones. El selector recibe ambos datos del target efectivo y devuelve una única variante; si no
existe, produce un diagnóstico que nombra SDK, arquitectura y versión. / `.nssdk` manifests can
declare `Target` variants with an architecture and inclusive version range. The selector receives
both values from the effective target and returns one variant; when none exists, its diagnostic
names the SDK, architecture, and version.

## Determinismo / Determinism

Las versiones son secuencias numéricas de hasta ocho componentes, con ceros finales equivalentes.
Los rangos invertidos o superpuestos para una misma arquitectura se rechazan al cargar el
manifiesto, antes de cualquier selección. / Versions are numeric sequences of up to eight
components, with equivalent trailing zeroes. Inverted or overlapping ranges for one architecture
are rejected while loading the manifest, before any selection.

## Pruebas / Tests

La prueba dirigida cubre x64 y arm64, dos generaciones de SDK, normalización de versión, ausencia
de arquitectura y solapamientos ambiguos. / The focused test covers x64 and arm64, two SDK
generations, version normalization, a missing architecture, and ambiguous overlaps.
