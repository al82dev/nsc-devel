# Plan de pruebas — Library.NativeSharp.Win32.Compiler

## Alcance

4 contratos mantenidos; 24 casos diseñados. No es una cuota: el conteo deriva del inventario 12B.

## Casos

| ID | Contrato | Clase | Objetivo | Oráculo | Frecuencia |
|---|---|---|---|---|---|
| `T12B-01045` | `C12B-W32-001` | `valid-minimal` | caso nominal mínimo: Frontera ABI Compiler Mapped File | éxito exacto y salida del productor; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01046` | `C12B-W32-001` | `valid-representative` | caso representativo completo: Frontera ABI Compiler Mapped File | salida, efectos y artefactos completos; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01047` | `C12B-W32-001` | `invalid-precondition` | productor o dependencia ausente: Frontera ABI Compiler Mapped File | fallo en la frontera propietaria y cero consumo downstream; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01048` | `C12B-W32-001` | `invalid-structure` | estructura, valor o referencia inválida: Frontera ABI Compiler Mapped File | diagnóstico estructural sin publicación parcial; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01049` | `C12B-W32-001` | `boundary` | vacío, singleton, duplicado y extremos: Frontera ABI Compiler Mapped File | resultado de borde documentado; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01050` | `C12B-W32-001` | `determinism` | repetición y orden independiente: Frontera ABI Compiler Mapped File | misma estructura/hash canónico o diferencia declarada; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `NIGHTLY` |
| `T12B-01051` | `C12B-W32-002` | `valid-minimal` | caso nominal mínimo: Frontera ABI Compiler Services | éxito exacto y salida del productor; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01052` | `C12B-W32-002` | `valid-representative` | caso representativo completo: Frontera ABI Compiler Services | salida, efectos y artefactos completos; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01053` | `C12B-W32-002` | `invalid-precondition` | productor o dependencia ausente: Frontera ABI Compiler Services | fallo en la frontera propietaria y cero consumo downstream; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01054` | `C12B-W32-002` | `invalid-structure` | estructura, valor o referencia inválida: Frontera ABI Compiler Services | diagnóstico estructural sin publicación parcial; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01055` | `C12B-W32-002` | `boundary` | vacío, singleton, duplicado y extremos: Frontera ABI Compiler Services | resultado de borde documentado; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01056` | `C12B-W32-002` | `determinism` | repetición y orden independiente: Frontera ABI Compiler Services | misma estructura/hash canónico o diferencia declarada; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `NIGHTLY` |
| `T12B-01057` | `C12B-W32-003` | `valid-minimal` | caso nominal mínimo: Frontera ABI Linker Adapters | éxito exacto y salida del productor; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01058` | `C12B-W32-003` | `valid-representative` | caso representativo completo: Frontera ABI Linker Adapters | salida, efectos y artefactos completos; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01059` | `C12B-W32-003` | `invalid-precondition` | productor o dependencia ausente: Frontera ABI Linker Adapters | fallo en la frontera propietaria y cero consumo downstream; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01060` | `C12B-W32-003` | `invalid-structure` | estructura, valor o referencia inválida: Frontera ABI Linker Adapters | diagnóstico estructural sin publicación parcial; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01061` | `C12B-W32-003` | `boundary` | vacío, singleton, duplicado y extremos: Frontera ABI Linker Adapters | resultado de borde documentado; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01062` | `C12B-W32-003` | `determinism` | repetición y orden independiente: Frontera ABI Linker Adapters | misma estructura/hash canónico o diferencia declarada; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `NIGHTLY` |
| `T12B-01063` | `C12B-W32-004` | `valid-minimal` | caso nominal mínimo: Proyecto distribuido Native Sharp.Win32.Compiler | éxito exacto y salida del productor; oracle: grafo de proyecto, unidades, referencias y artefactos publicados | `PR` |
| `T12B-01064` | `C12B-W32-004` | `valid-representative` | caso representativo completo: Proyecto distribuido Native Sharp.Win32.Compiler | salida, efectos y artefactos completos; oracle: grafo de proyecto, unidades, referencias y artefactos publicados | `PR` |
| `T12B-01065` | `C12B-W32-004` | `invalid-precondition` | productor o dependencia ausente: Proyecto distribuido Native Sharp.Win32.Compiler | fallo en la frontera propietaria y cero consumo downstream; oracle: grafo de proyecto, unidades, referencias y artefactos publicados | `PR` |
| `T12B-01066` | `C12B-W32-004` | `invalid-structure` | estructura, valor o referencia inválida: Proyecto distribuido Native Sharp.Win32.Compiler | diagnóstico estructural sin publicación parcial; oracle: grafo de proyecto, unidades, referencias y artefactos publicados | `PR` |
| `T12B-01067` | `C12B-W32-004` | `boundary` | vacío, singleton, duplicado y extremos: Proyecto distribuido Native Sharp.Win32.Compiler | resultado de borde documentado; oracle: grafo de proyecto, unidades, referencias y artefactos publicados | `PR` |
| `T12B-01068` | `C12B-W32-004` | `determinism` | repetición y orden independiente: Proyecto distribuido Native Sharp.Win32.Compiler | misma estructura/hash canónico o diferencia declarada; oracle: grafo de proyecto, unidades, referencias y artefactos publicados | `NIGHTLY` |