# Plan de pruebas — SDK

## Alcance

1 contratos mantenidos; 6 casos diseñados. No es una cuota: el conteo deriva del inventario 12B.

## Casos

| ID | Contrato | Clase | Objetivo | Oráculo | Frecuencia |
|---|---|---|---|---|---|
| `T12B-01075` | `C12B-SDK-001` | `valid-minimal` | caso nominal mínimo: Cierre de imports del runtime Win64 | éxito exacto y salida del productor; oracle: conjunto exacto DLL:símbolo y smoke de enlace/ejecución | `PR` |
| `T12B-01076` | `C12B-SDK-001` | `valid-representative` | caso representativo completo: Cierre de imports del runtime Win64 | salida, efectos y artefactos completos; oracle: conjunto exacto DLL:símbolo y smoke de enlace/ejecución | `PR` |
| `T12B-01077` | `C12B-SDK-001` | `invalid-precondition` | productor o dependencia ausente: Cierre de imports del runtime Win64 | fallo en la frontera propietaria y cero consumo downstream; oracle: conjunto exacto DLL:símbolo y smoke de enlace/ejecución | `PR` |
| `T12B-01078` | `C12B-SDK-001` | `invalid-structure` | estructura, valor o referencia inválida: Cierre de imports del runtime Win64 | diagnóstico estructural sin publicación parcial; oracle: conjunto exacto DLL:símbolo y smoke de enlace/ejecución | `PR` |
| `T12B-01079` | `C12B-SDK-001` | `boundary` | vacío, singleton, duplicado y extremos: Cierre de imports del runtime Win64 | resultado de borde documentado; oracle: conjunto exacto DLL:símbolo y smoke de enlace/ejecución | `PR` |
| `T12B-01080` | `C12B-SDK-001` | `determinism` | repetición y orden independiente: Cierre de imports del runtime Win64 | misma estructura/hash canónico o diferencia declarada; oracle: conjunto exacto DLL:símbolo y smoke de enlace/ejecución | `NIGHTLY` |