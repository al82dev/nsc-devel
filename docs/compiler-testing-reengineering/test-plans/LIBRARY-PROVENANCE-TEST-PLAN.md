# Plan de pruebas — Library.Provenance

## Alcance

1 contratos mantenidos; 6 casos diseñados. No es una cuota: el conteo deriva del inventario 12B.

## Casos

| ID | Contrato | Clase | Objetivo | Oráculo | Frecuencia |
|---|---|---|---|---|---|
| `T12B-00469` | `C12B-PRV-001` | `valid-minimal` | caso nominal mínimo: Procedencia del árbol lib | éxito exacto y salida del productor; oracle: texto exacto y presencia en manifiesto de distribución | `PR` |
| `T12B-00470` | `C12B-PRV-001` | `valid-representative` | caso representativo completo: Procedencia del árbol lib | salida, efectos y artefactos completos; oracle: texto exacto y presencia en manifiesto de distribución | `PR` |
| `T12B-00471` | `C12B-PRV-001` | `invalid-precondition` | productor o dependencia ausente: Procedencia del árbol lib | fallo en la frontera propietaria y cero consumo downstream; oracle: texto exacto y presencia en manifiesto de distribución | `PR` |
| `T12B-00472` | `C12B-PRV-001` | `invalid-structure` | estructura, valor o referencia inválida: Procedencia del árbol lib | diagnóstico estructural sin publicación parcial; oracle: texto exacto y presencia en manifiesto de distribución | `PR` |
| `T12B-00473` | `C12B-PRV-001` | `boundary` | vacío, singleton, duplicado y extremos: Procedencia del árbol lib | resultado de borde documentado; oracle: texto exacto y presencia en manifiesto de distribución | `PR` |
| `T12B-00474` | `C12B-PRV-001` | `determinism` | repetición y orden independiente: Procedencia del árbol lib | misma estructura/hash canónico o diferencia declarada; oracle: texto exacto y presencia en manifiesto de distribución | `NIGHTLY` |