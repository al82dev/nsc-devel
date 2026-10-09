# Plan de pruebas — Packaging

## Alcance

3 contratos mantenidos; 18 casos diseñados. No es una cuota: el conteo deriva del inventario 12B.

## Casos

| ID | Contrato | Clase | Objetivo | Oráculo | Frecuencia |
|---|---|---|---|---|---|
| `T12B-00457` | `C12B-PKG-001` | `valid-minimal` | caso nominal mínimo: Instalación o empaquetado deploy | éxito exacto y salida del productor; oracle: comando, identidad, árbol instalado/archive y código de salida | `PR` |
| `T12B-00458` | `C12B-PKG-001` | `valid-representative` | caso representativo completo: Instalación o empaquetado deploy | salida, efectos y artefactos completos; oracle: comando, identidad, árbol instalado/archive y código de salida | `PR` |
| `T12B-00459` | `C12B-PKG-001` | `invalid-precondition` | productor o dependencia ausente: Instalación o empaquetado deploy | fallo en la frontera propietaria y cero consumo downstream; oracle: comando, identidad, árbol instalado/archive y código de salida | `PR` |
| `T12B-00460` | `C12B-PKG-001` | `invalid-structure` | estructura, valor o referencia inválida: Instalación o empaquetado deploy | diagnóstico estructural sin publicación parcial; oracle: comando, identidad, árbol instalado/archive y código de salida | `PR` |
| `T12B-00461` | `C12B-PKG-001` | `boundary` | vacío, singleton, duplicado y extremos: Instalación o empaquetado deploy | resultado de borde documentado; oracle: comando, identidad, árbol instalado/archive y código de salida | `PR` |
| `T12B-00462` | `C12B-PKG-001` | `determinism` | repetición y orden independiente: Instalación o empaquetado deploy | misma estructura/hash canónico o diferencia declarada; oracle: comando, identidad, árbol instalado/archive y código de salida | `NIGHTLY` |
| `T12B-00463` | `C12B-PKG-002` | `valid-minimal` | caso nominal mínimo: Instalación o empaquetado deploy | éxito exacto y salida del productor; oracle: comando, identidad, árbol instalado/archive y código de salida | `PR` |
| `T12B-00464` | `C12B-PKG-002` | `valid-representative` | caso representativo completo: Instalación o empaquetado deploy | salida, efectos y artefactos completos; oracle: comando, identidad, árbol instalado/archive y código de salida | `PR` |
| `T12B-00465` | `C12B-PKG-002` | `invalid-precondition` | productor o dependencia ausente: Instalación o empaquetado deploy | fallo en la frontera propietaria y cero consumo downstream; oracle: comando, identidad, árbol instalado/archive y código de salida | `PR` |
| `T12B-00466` | `C12B-PKG-002` | `invalid-structure` | estructura, valor o referencia inválida: Instalación o empaquetado deploy | diagnóstico estructural sin publicación parcial; oracle: comando, identidad, árbol instalado/archive y código de salida | `PR` |
| `T12B-00467` | `C12B-PKG-002` | `boundary` | vacío, singleton, duplicado y extremos: Instalación o empaquetado deploy | resultado de borde documentado; oracle: comando, identidad, árbol instalado/archive y código de salida | `PR` |
| `T12B-00468` | `C12B-PKG-002` | `determinism` | repetición y orden independiente: Instalación o empaquetado deploy | misma estructura/hash canónico o diferencia declarada; oracle: comando, identidad, árbol instalado/archive y código de salida | `NIGHTLY` |
| `T12B-01069` | `C12B-PKG-003` | `valid-minimal` | caso nominal mínimo: Instalación o empaquetado package native sharp | éxito exacto y salida del productor; oracle: comando, identidad, árbol instalado/archive y código de salida | `PR` |
| `T12B-01070` | `C12B-PKG-003` | `valid-representative` | caso representativo completo: Instalación o empaquetado package native sharp | salida, efectos y artefactos completos; oracle: comando, identidad, árbol instalado/archive y código de salida | `PR` |
| `T12B-01071` | `C12B-PKG-003` | `invalid-precondition` | productor o dependencia ausente: Instalación o empaquetado package native sharp | fallo en la frontera propietaria y cero consumo downstream; oracle: comando, identidad, árbol instalado/archive y código de salida | `PR` |
| `T12B-01072` | `C12B-PKG-003` | `invalid-structure` | estructura, valor o referencia inválida: Instalación o empaquetado package native sharp | diagnóstico estructural sin publicación parcial; oracle: comando, identidad, árbol instalado/archive y código de salida | `PR` |
| `T12B-01073` | `C12B-PKG-003` | `boundary` | vacío, singleton, duplicado y extremos: Instalación o empaquetado package native sharp | resultado de borde documentado; oracle: comando, identidad, árbol instalado/archive y código de salida | `PR` |
| `T12B-01074` | `C12B-PKG-003` | `determinism` | repetición y orden independiente: Instalación o empaquetado package native sharp | misma estructura/hash canónico o diferencia declarada; oracle: comando, identidad, árbol instalado/archive y código de salida | `NIGHTLY` |