# Plan de pruebas — Library.NativeSharp.LLVM.C

## Alcance

4 contratos mantenidos; 24 casos diseñados. No es una cuota: el conteo deriva del inventario 12B.

## Casos

| ID | Contrato | Clase | Objetivo | Oráculo | Frecuencia |
|---|---|---|---|---|---|
| `T12B-01021` | `C12B-LVC-001` | `valid-minimal` | caso nominal mínimo: Frontera ABI LLVMApi | éxito exacto y salida del productor; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01022` | `C12B-LVC-001` | `valid-representative` | caso representativo completo: Frontera ABI LLVMApi | salida, efectos y artefactos completos; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01023` | `C12B-LVC-001` | `invalid-precondition` | productor o dependencia ausente: Frontera ABI LLVMApi | fallo en la frontera propietaria y cero consumo downstream; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01024` | `C12B-LVC-001` | `invalid-structure` | estructura, valor o referencia inválida: Frontera ABI LLVMApi | diagnóstico estructural sin publicación parcial; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01025` | `C12B-LVC-001` | `boundary` | vacío, singleton, duplicado y extremos: Frontera ABI LLVMApi | resultado de borde documentado; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01026` | `C12B-LVC-001` | `determinism` | repetición y orden independiente: Frontera ABI LLVMApi | misma estructura/hash canónico o diferencia declarada; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `NIGHTLY` |
| `T12B-01027` | `C12B-LVC-002` | `valid-minimal` | caso nominal mínimo: Frontera ABI LLVMEnums | éxito exacto y salida del productor; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01028` | `C12B-LVC-002` | `valid-representative` | caso representativo completo: Frontera ABI LLVMEnums | salida, efectos y artefactos completos; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01029` | `C12B-LVC-002` | `invalid-precondition` | productor o dependencia ausente: Frontera ABI LLVMEnums | fallo en la frontera propietaria y cero consumo downstream; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01030` | `C12B-LVC-002` | `invalid-structure` | estructura, valor o referencia inválida: Frontera ABI LLVMEnums | diagnóstico estructural sin publicación parcial; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01031` | `C12B-LVC-002` | `boundary` | vacío, singleton, duplicado y extremos: Frontera ABI LLVMEnums | resultado de borde documentado; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01032` | `C12B-LVC-002` | `determinism` | repetición y orden independiente: Frontera ABI LLVMEnums | misma estructura/hash canónico o diferencia declarada; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `NIGHTLY` |
| `T12B-01033` | `C12B-LVC-003` | `valid-minimal` | caso nominal mínimo: Frontera ABI LLVMVersion Policy | éxito exacto y salida del productor; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01034` | `C12B-LVC-003` | `valid-representative` | caso representativo completo: Frontera ABI LLVMVersion Policy | salida, efectos y artefactos completos; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01035` | `C12B-LVC-003` | `invalid-precondition` | productor o dependencia ausente: Frontera ABI LLVMVersion Policy | fallo en la frontera propietaria y cero consumo downstream; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01036` | `C12B-LVC-003` | `invalid-structure` | estructura, valor o referencia inválida: Frontera ABI LLVMVersion Policy | diagnóstico estructural sin publicación parcial; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01037` | `C12B-LVC-003` | `boundary` | vacío, singleton, duplicado y extremos: Frontera ABI LLVMVersion Policy | resultado de borde documentado; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `PR` |
| `T12B-01038` | `C12B-LVC-003` | `determinism` | repetición y orden independiente: Frontera ABI LLVMVersion Policy | misma estructura/hash canónico o diferencia declarada; oracle: firma física, símbolos/imports, argumentos, recursos y código de salida | `NIGHTLY` |
| `T12B-01039` | `C12B-LVC-004` | `valid-minimal` | caso nominal mínimo: Proyecto distribuido Native Sharp.LLVM.C | éxito exacto y salida del productor; oracle: grafo de proyecto, unidades, referencias y artefactos publicados | `PR` |
| `T12B-01040` | `C12B-LVC-004` | `valid-representative` | caso representativo completo: Proyecto distribuido Native Sharp.LLVM.C | salida, efectos y artefactos completos; oracle: grafo de proyecto, unidades, referencias y artefactos publicados | `PR` |
| `T12B-01041` | `C12B-LVC-004` | `invalid-precondition` | productor o dependencia ausente: Proyecto distribuido Native Sharp.LLVM.C | fallo en la frontera propietaria y cero consumo downstream; oracle: grafo de proyecto, unidades, referencias y artefactos publicados | `PR` |
| `T12B-01042` | `C12B-LVC-004` | `invalid-structure` | estructura, valor o referencia inválida: Proyecto distribuido Native Sharp.LLVM.C | diagnóstico estructural sin publicación parcial; oracle: grafo de proyecto, unidades, referencias y artefactos publicados | `PR` |
| `T12B-01043` | `C12B-LVC-004` | `boundary` | vacío, singleton, duplicado y extremos: Proyecto distribuido Native Sharp.LLVM.C | resultado de borde documentado; oracle: grafo de proyecto, unidades, referencias y artefactos publicados | `PR` |
| `T12B-01044` | `C12B-LVC-004` | `determinism` | repetición y orden independiente: Proyecto distribuido Native Sharp.LLVM.C | misma estructura/hash canónico o diferencia declarada; oracle: grafo de proyecto, unidades, referencias y artefactos publicados | `NIGHTLY` |