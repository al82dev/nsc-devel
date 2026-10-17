# NSC 3.0.5 — L06A: telemetría LLVM fría automática

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`,
  `66f1bf3f54f4519a93c56dc39671f06d473b9fe7`.
- **Evidencia causal:** el proyecto real reporta 78.989 ms en `llvm-object-generation` para 15
  unidades, pero la GUI sólo pasa `--progress`; la telemetría existente requiere `--time` y se
  imprime después del enlace.
- **Hipótesis única:** registrar los contadores ya existentes al terminar cada unidad permite
  atribuir el costo frío real sin cambiar la GUI y conserva evidencia si una etapa posterior falla.
- **Naturaleza:** instrumentación temporal de diagnóstico, no optimización ni cambio funcional.
- **Archivos autorizados:** `include/nativesharp/LLVM/ProjectOopObjectGenerator.h`,
  `src/LLVM/ProjectOopObjectGenerator.cpp`, `src/Driver/Driver.cpp`, esta evidencia y tracking.
- **Salida autorizada:** `A:\temp\nsc-llvm-code-unit-timing.log`, truncada al iniciar una generación,
  serializada por mutex y vaciada tras cada unidad.
- **Datos:** nombre de unidad, hit/miss y razón, microsegundos de fingerprint, lookup, lowering,
  creación/configuración del target, optimizer, emission, verification y store, más error causal
  si existe. El tiempo de target se agrega también al reporte explícito `--time` y al peso histórico
  de la partición para que no quede costo LLVM sin atribuir.
- **Contratos congelados:** objetos, orden lógico, caché, paralelismo, CLI, stdout/stderr, GUI,
  sintaxis, semántica, API, ABI y NSMETA.
- **Prohibido:** tocar Builder/GUI, agregar argumentos, estimar el culpable sin el log, baseline,
  suite completa, deploy o instalación.
- **Gate:** build focal `nsc`, hello cold/warm, log legible y completo aun cuando el enlace falle;
  commit local aislado. L06 permanece abierto hasta perfilar y corregir el proyecto real.

## Estado

Instrumentación terminada y validada. L06 permanece abierto: esta tanda sólo produce la evidencia
causal necesaria para corregir el costo LLVM frío del proyecto real.

## Implementación

- `CodeUnitTelemetry` contabiliza de forma separada `targetMicros`, que cubre la creación de
  `LlvmTargetMachine` y `configureModuleForTarget`.
- El reporte explícito `--time`, el peso histórico de partición y el log temporal incluyen ese
  contador; no queda absorbido dentro de lowering ni de optimizer.
- Cada worker escribe su registro al salir de `processModule`, bajo mutex y con `flush`; las
  unidades ya terminadas sobreviven a un error posterior de otra unidad o del enlazador.
- El archivo se abre con truncación por invocación y su encabezado identifica inequívocamente
  `NSC305-L06 LLVM CODE UNIT INSTRUMENTATION version=1`.
- La instrumentación no modifica stdout/stderr, argumentos, Builder ni GUI. Si el archivo no se
  puede abrir, la compilación continúa sin cambiar su resultado.

## Evidencia focal

- Build focal: `cmake --build build-test-infrastructure --target nsc -j 16` — terminado; contrato
  de runtime estático de `nsc.exe`: `passed`.
- Pruebas focales: `nsc.compilation-artifact-cache` y
  `nsc.compilation-artifact-cache-driver` — `2/2 PASS`.
- Hello O0 con line tables y caché vacía: tres unidades con `miss=1`; el log atribuyó
  fingerprint, lookup, lowering, target, optimizer, emission, verification y store en las tres.
- Repetición idéntica: tres unidades con `hit=1` y lowering, target, optimizer, emission y store
  iguales a cero.
- Evidencia conservada en `A:\temp\nsc-l06a-cold.log` y
  `A:\temp\nsc-l06a-warm.log`.
- Ambas invocaciones alcanzaron y terminaron `llvm-object-generation`. El enlace focal falló
  después por no poder abrir la biblioteca runtime de una instalación externa en C:; esa falla no
  invalida el gate de esta tanda y no se modificó esa instalación.

## Uso para cerrar L06

Compilar el proyecto real normalmente desde su GUI con un `nsc.exe` que contenga esta tanda y
recuperar `A:\temp\nsc-llvm-code-unit-timing.log`. Los tiempos por unidad son concurrentes: no se
deben sumar como tiempo de pared. Se debe comparar la cola crítica y las unidades rezagadas, además
de verificar hits/misses. L06 sólo puede cerrarse después de corregir el tramo dominante y repetir
la medición real.
