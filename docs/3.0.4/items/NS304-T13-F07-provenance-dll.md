# NS304-T13-F07 — Provenance intermodular/DLL

- **Estado**: CERRADO — provenance funcional en drop normal, unwind de constructor y frontera DLL.
- **Rama**: development

## SCOPE LOCK de reanudación

- **Revisión base**: `3823dbef496ffe078593faf8d4a3a9cdd2a730bd` sobre `development`; el commit
  `06c3a700` (`temporal`) y toda la secuencia previa de DeepSeek se preservan sin reescritura.
- **Objetivo**: cerrar exclusivamente los residuales que el expediente dejó explícitos: liberar con
  la misma provenance cuando el constructor lanza, transferir ownership de forma válida y probar el
  header `context + thunk` entre producer/consumer.
- **Archivos permitidos**: `ProjectOopLlvmLowerer.cpp`, casos focales `TL-LANG-492..494`, sus dos
  inventarios y manifiesto, este expediente, tracking maestro y `A:\restantes-deepseek.md`.
- **Archivos protegidos**: T15-F01 ya cerrado, parser/binder no necesarios para la transferencia
  owned existente, Builder/CMake, baseline, deploy, demás ítems y todo versionamiento NSMETA.
- **Validación autorizada**: recompilar `nsc` y `nsc_component_tests` sólo en targets necesarios;
  ejecutar individualmente `TL-LANG-493`, `TL-LANG-494` y el guard `TL-LANG-492`, siempre O0 y con
  temporales/artefactos en `A:`. No ejecutar batería, componente completo ni baseline.
- **Criterio de cierre**: repro previo demostrado, tres casos focales PASS, regla de vida del allocator
  documentada, tracking/lista coherentes y commit local aislado.

## Hueco concreto (demostrado)

`owned A a = new (alloc) A(42);` → al droppear `a`, `alloc.Deallocate` no se llamaba (`Deallocs==0`).
**Cerrado**: ahora `Deallocs==1` y `Drops==1`; el objeto conserva el allocator en su header.

## Implementado (commits)

- `74ce52ef` FASE 1: contrato ABI (header prependido: descriptor@0, context@8, thunk@16; payload@24).
- `dced1bfb` FASE 2: `ClassObjectLayout` + offsets + tamaño (+16B) con autoridad única; herencia/campos OK.
- `7726a11e` FASE 3a: provenance default (null + free thunk) en construcción; thunk `void(void*,void*)` internal.
- `8c8f54c1` FASE 3b+4: provenance custom (context + thunk por tipo de allocator) + deallocación unificada (campo 7 + thunk).
- `5c4a11bb` test focal TL-LANG-492 (drop llama `Deallocate` del mismo allocator; herencia + no-doble-free).
- Cierre de reanudación: el caller instala un `cleanuppad` alrededor del constructor de
  `new (allocator) T(...)`; si el constructor desenrolla, carga `context + thunk` del header,
  libera exclusivamente el bloque y continúa el unwind sin ejecutar el destructor del objeto
  incompleto.
- TL-LANG-493 cubre el fallo de constructor después de `Allocate`.
- TL-LANG-494 construye un producer `SharedLibrary`, lo consume mediante
  `LibraryReference Include="T13F07Producer.lib" Runtime="T13F07Producer.dll"`, transfiere el
  objeto con `return move(product)` y ejecuta el drop en el EXE consumer.

## Verificado O0

- **Repro anterior a la corrección**: el fixture focal compiló (`exit 0`) y su ejecutable terminó
  `exit 1`, porque el constructor lanzó después de `Allocate` y `Deallocs` permaneció en cero.
- **TL-LANG-493**: PASS, `exit 0`; el unwind del constructor llama una vez al deallocator de la
  misma provenance.
- **TL-LANG-494**: PASS, `exit 0`; DLL producer + EXE consumer, `Value==42` y `Deallocs==1` después
  de la transferencia `owned` explícita.
- **TL-LANG-492** (guard): PASS, `exit 0`; conserva drop normal, herencia y ausencia de doble free.
- Los tres casos finales se ejecutaron individualmente con `-O0 -g` mediante
  `emitT01Product`, usando `nsc.exe` SHA-256
  `1CCB16BA661BD9E5B9084C0DFD1FE6B3083F29CD5EA2FCF080CF3814A7D94CBD` y runner Debug SHA-256
  `11EE143967845DE06B69CA6A8A2C25B3D704F0A1B45AF53EBB2CB2E328BAB994`.

## Cierre de los residuales

1. **Fallo de constructor**: cerrado por el cleanup caller-side y TL-LANG-493.
2. **Escape/retorno**: cerrado mediante la sintaxis vigente de transferencia: primero se nombra el owner
   local y después se devuelve con `return move(product)`. El intento inicial de la fixture con retorno
   directo fue rechazado correctamente; no justificó modificar parser, binder ni ownership.
3. **Cross-module/DLL**: cerrado por TL-LANG-494 contra una DLL real, no sólo una biblioteca estática.
4. **Vida del allocator y del módulo**: el header guarda un contexto crudo y un puntero a thunk. El allocator
   debe permanecer vivo hasta después del último drop de los objetos que lo referencian; asimismo, la DLL que
   contiene el thunk debe continuar cargada hasta que esos objetos hayan sido liberados. TL-LANG-494 mantiene
   ambos vivos durante el drop del consumer.

## ABI de provenance (fijo)

`void deallocate_thunk(void* context, void* ptr)`; default `context=null, thunk=free`; custom
`((T*)context)->Deallocate((byte*)ptr)` generado por tipo de allocator (internal linkage).

## Frontera y validación no ejecutada

- No se introdujo ni reactivó versionado NSMETA, compatibilidad N/N-1 ni un gate de metadata independiente.
- La metadata emitida por el producer DLL se consumió únicamente como parte del flujo funcional existente de
  `LibraryReference`.
- No se ejecutaron baseline, deploy, componente completo ni batería global. Esa validación acumulada queda para
  la GUI del usuario después de terminar las tandas.

## Corrección de catálogo posterior al cierre — 2026-10-15

- La entrada `TL-LANG-493` se registró inicialmente con `class: exceptional`, valor que no pertenece al
  vocabulario canónico de `CASE_CLASSES` y bloqueaba la carga de NSC Builder 3 antes de mostrar la GUI.
- Se corrigió exclusivamente a `class: nominal`, coherente con los casos exitosos que ejercitan rutas de
  excepción (`TL-LANG-407`, `TL-LANG-491` y `TL-LANG-496`). La semántica, el oracle y la prueba no cambiaron.
- La validación de esta corrección se limita a la carga íntegra del manifiesto y a las pruebas focales de su
  contrato; no constituye una ejecución de la batería.
