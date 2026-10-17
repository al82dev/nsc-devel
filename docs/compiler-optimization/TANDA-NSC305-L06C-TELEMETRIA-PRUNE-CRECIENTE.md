# NSC 3.0.5 — L06C: telemetría del barrido creciente de caché

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`,
  `0b13bc5b4a932fd2fc797d374a130914aa254b1e`.
- **Evidencia estructural:** la caché predeterminada llama `CompilationArtifactCache::prune`
  antes de cada emisión EXE/LIB/DLL. `collectPrunableEntries` recorre recursivamente todos los
  objetos y planes, consulta tamaño y fecha y ordena todas las entradas aun cuando el total esté
  por debajo de 2 GiB.
- **Síntoma autorizado:** el usuario observa que compilaciones consecutivas del mismo proyecto
  aumentan de tiempo; la fase visible LLVM no explica por sí sola todo el tiempo de pared.
- **Hipótesis única:** el costo O(N log N) del prune puede crecer con N y permanece oculto porque
  se ejecuta antes de las fases publicadas. Primero se mide; esta tanda no cambia su algoritmo.
- **Naturaleza:** instrumentación temporal no funcional.
- **Archivos autorizados:** `include/nativesharp/LLVM/CompilationArtifactCache.h`,
  `src/LLVM/CompilationArtifactCache.cpp`, `src/LLVM/ProjectOopObjectGenerator.cpp`,
  este expediente y tracking.
- **Datos autorizados:** prune ejecutado/no ejecutado, microsegundos, entradas y bytes
  inspeccionados, entradas y bytes expulsados.
- **Salida:** encabezado versión 2 de `A:\temp\nsc-llvm-code-unit-timing.log`; no se agrega
  salida a consola, ni siquiera bajo `--progress` o `--time`.
- **Contratos congelados:** claves, almacenamiento, restore, prune, límite, orden de expulsión,
  CLI, stdout normal, GUI, objetos, sintaxis, semántica, API, ABI y NSMETA.
- **Pruebas protegidas:** no se modifican fixtures ni expectativas existentes.
- **Prohibido:** optimizar prune sin evidencia real, tocar Builder/GUI, baseline, suite completa,
  deploy, instalación, push, pull o fetch.
- **Gate:** build focal `nsc`; dos pruebas focales de caché; hello/default-cache con telemetría
  legible; diferencial auditado y commit local aislado. L06 continúa abierto.

## Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | telemetría interna de cache prune |
| Perfiles | O0 focal; comportamiento común sin alteración |
| Salidas | log temporal de diagnóstico |
| Comportamiento | observación exclusivamente |

## Estado

Implementación y validación terminadas el 17 de octubre de 2026.

### Cambio aplicado

- `CompilationArtifactCache::prune` conserva exactamente su recorrido, orden y política de
  expulsión, pero registra duración, entradas y bytes inspeccionados y expulsados.
- El encabezado de `A:\temp\nsc-llvm-code-unit-timing.log` avanzó a esquema 2 e incluye las
  seis magnitudes del prune. No se añadió ningún argumento ni salida normal nueva.
- La instrumentación no publica una fase nueva en consola: la GUI puede seguir usando
  `--progress` sin desbordar su salida.

### Validación focal

- Build focal `nsc` y `nsc_compilation_artifact_cache_tests`: **PASS**. El contrato estático de
  `nsc.exe` también pasó.
- `nsc.compilation-artifact-cache`: **PASS**.
- `nsc.compilation-artifact-cache-driver`: **PASS**.
- Hello O0 con caché predeterminada: dos ejecuciones del log midieron **834 us** y **702 us**
  para **7 entradas / 13,225 bytes**, sin expulsiones. La segunda se ejecutó sólo con
  `--progress` y confirmó que no aparece una fase nueva en consola. Las tres unidades LLVM se
  restauraron de caché; el enlace posterior no se consideró parte del gate porque la
  instalación externa de runtime no estaba disponible para el fixture aislado.
- `git diff --check`: **PASS**. El build focal compiló con la configuración de advertencias del
  proyecto sin diagnósticos.

### Conclusión

La medición queda lista, pero el ejemplo pequeño no demuestra que prune explique los **123 s**
del proyecto real. L06 permanece abierto. La siguiente ejecución GUI debe aportar el log real;
sólo entonces se autoriza decidir si se optimiza el recorrido O(N log N) o si el costo dominante
continúa dentro de lowering, optimizer, emisión, verificación o almacenamiento por unidad.
