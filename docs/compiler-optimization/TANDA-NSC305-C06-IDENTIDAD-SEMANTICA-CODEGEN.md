# NSC 3.0.5 — C06: identidad semántica de codegen para caché de objetos

## SCOPE LOCK

- **Repositorio/rama:** `A:\nsc-devel`, `development`.
- **Defecto causal:** al compilar un juego, `Driver` configura `CompilationArtifactCache` con
  `kCompilerBuildId`. Ese ID es aleatorio y cambia en cada construcción de `nsc.exe`; por tanto,
  instalar cualquier build nuevo invalida todos los objetos LLVM/COFF del juego, aunque el cambio
  haya sido sólo de parser, driver o documentación y el MIR sea idéntico.
- **Archivos autorizados:** generación CMake de identidad, una plantilla generada bajo
  `include/nativesharp/LLVM`, `Driver.cpp`, `CompilationArtifactCache` y sus pruebas focales, este
  expediente y tracking externo.
- **Contratos congelados:** identidad visible única de cada `nsc.exe`, banner/version, contenido de
  objetos, LLVM/COFF verification, sintaxis, semántica, API pública, ABI, NSMETA, CLI y artefactos
  3.0.4.
- **Prohibido:** reutilizar por versión comercial solamente, eliminar invalidación del backend,
  omitir LLVM/target/policy/O-level/debug/MIR de la clave, aceptar objetos sin verificación, tocar
  el formato NSMETA, ejecutar baseline, suite completa, deploy o instalación.
- **Gate:** dos build IDs distintos con las mismas fuentes productoras comparten identidad de
  codegen; modificar la identidad de codegen cambia la clave; las variaciones ya cubiertas de
  MIR/LLVM/policy/O/debug siguen cambiándola; prueba focal PASS; `nsc` construye.
- **Límite honesto:** C06 reduce recompilaciones warm después de instalar un `nsc` equivalente en
  codegen. No reduce por sí solo el costo cold y no se presentará como solución de 1:52.

## Diseño

1. CMake calcula SHA-256 reproducible de paths relativos y SHA-256 de fuentes/headers de
   AST/Language/Frontend/Target/LLVM que pueden alterar MIR o emisión de objetos.
2. El header generado expone esa huella separada del build ID único del ejecutable.
3. La caché renombra su contrato de `toolchainIdentity` a `codegenIdentity` y sube el schema de la
   clave una vez.
4. `Driver` usa exclusivamente la identidad de codegen para objetos del juego.
5. LLVM version, target, codegen policy, optimization, pipeline, debug y MIR/IR siguen siendo
   componentes independientes de la clave.

## Estado

C06 terminado el 2026-10-17.

## Implementación

- CMake calcula `kCodegenCacheIdentity` como SHA-256 de paths relativos y contenido de la frontera
  AST/Language/Frontend/Target/LLVM.
- La identidad visible y aleatoria de cada `nsc.exe` permanece intacta y continúa apareciendo en
  banner, diagnostics y bundles.
- `CompilationArtifactCache` recibe `setCodegenIdentity`; los schemas de objeto/input suben de 3
  a 4 y la clave etiqueta explícitamente `codegen`.
- `Driver` deja de usar `kCompilerBuildId` para objetos del programa y usa la huella semántica.

## Resultados

- Prueba unitaria focal: `PASS: 23 checks`.
- Identidad generada: SHA-256 canónico de 64 hexadecimales.
- Primer build de `nsc`: hash `7e53fb71646a2a15`; hello O0/g0 produjo 3 misses cold.
- Segundo build de `nsc`: hash `b3a677ce6487a6dd`; la identidad de codegen permaneció igual y
  el mismo hello restauró 3/3 objetos LLVM como hits.
- LLVM focal: 29 ms cold y 17 ms con restore; ambas pasadas alcanzaron el enlace.
- El enlace focal terminó con error porque `nslink.exe` no está junto al build ni en `PATH`; esto
  ocurre después del store/restore verificado y no invalida el gate de objetos.
- Construcción target `nsc`: PASS; contrato de host runtime estático: PASS.
- `git diff --check`: PASS.

No se ejecutaron baseline, suite completa, deploy ni instalación. La compilación del juego aportada
por el usuario mostró además 78.989 ms en LLVM, 8.521 ms en enlace y caché deshabilitada (0 hits,
0 misses); ese defecto se continúa en C07/C08/L06.
