# Tanda baseline 03 — Matriz seleccionable y reutilización exacta

## Objetivo

Evitar repeticiones innecesarias y permitir corridas dirigidas de uno o varios enlazadores y
perfiles O0–O3, sin permitir que una selección parcial se presente como baseline certificado.

## Selección de matriz

La página Gate 0 permite elegir:

- ambos enlazadores, sólo LLD-link o sólo NSLink;
- cualquier combinación no vacía de O0, O1, O2 y O3;
- reutilizar o forzar la repetición de resultados idénticos.

La consola acepta las opciones equivalentes:

- `--linker both|lld-link|NSLink`;
- `--optimization O0|O1|O2|O3`, repetible;
- `--no-reuse-results`.

## Certificación fail-closed

- Sólo `lld-link + NSLink + O0 + O1 + O2 + O3` establece `certify_baseline=true`.
- Una selección reducida escribe `scoped-validation.json` dentro de su historial.
- Una selección reducida no escribe `_abi/gates/G00`, no queda certificada y no llama a la
  promoción hacia `C:\native-sharp`, aunque todas sus pruebas pasen.
- El modo del host de Gate 0 continúa siendo `STATIC`.

## Reutilización sin falsos positivos

### CTest

Una pasada aprobada se reutiliza sólo si coincide una identidad SHA-256 que incluye:

- manifiesto canónico del árbol fuente;
- contenido de `nsc.exe` y del enlazador efectivo;
- `CTestTestfile.cmake`;
- contenido de todos los ejecutables del árbol CTest;
- selección exacta de nombres CTest.

La evidencia registra `status=reused`, la identidad y la corrida de origen. Cualquier cambio
produce una identidad nueva y obliga a ejecutar CTest.

### Fixtures y bug triage

- La identidad incremental deja de depender de timestamps y usa el contenido de fuentes,
  proyectos, compilador y enlazador.
- El hash de herramientas se calcula una vez por corrida y se comparte entre trabajadores.
- Una ejecución aprobada se reutiliza sólo si el `.exe`, el código esperado y todas las DLL
  adyacentes conservan exactamente el mismo contenido.
- El manifiesto distingue compilación incremental de ejecución reutilizada.

### Identidad reproducible del baseline

Cuando Gate 0 no recibe un `build-id` explícito, deriva uno estable de los primeros 16 dígitos del
digest de fuentes. Dos corridas sobre el mismo árbol pueden producir el mismo `nsc.exe`; un build
ordinario fuera del baseline conserva su identidad única.

## Redundancia eliminada

- No se vuelve a ejecutar un CTest completo ya aprobado bajo la misma identidad.
- No se vuelve a ejecutar un fixture cuyo ejecutable y runtime sean idénticos.
- Tocar un archivo sin cambiar su contenido ya no invalida por sí solo el caché incremental.
- La barra de progreso ahora avanza también al terminar una pasada CTest normal, igual que al
  reutilizarla.

## Archivos modificados

- `nscbuilder.py`
- `tests/selfhost/test_bootstrap_infrastructure.py`

## Archivo nuevo

- Este documento, que conserva alcance, garantías, archivos y validación de la tanda.

## Validación específica

- Seis pruebas unitarias dirigidas de matriz, fail-closed e identidades: aprobadas.
- Ayuda CLI: expone enlace, optimización y control de reutilización.
- Fixture real `c1_constants` O0 con LLD-link:
  - primera pasada: compilado, ejecutado y aprobado con exit 42;
  - segunda pasada: `reused`, exit 42, sin volver a ejecutarlo.
- Probe y artefactos temporales: eliminados.
- Baseline completo: no ejecutado.
- `C:\native-sharp`: no tocado.

