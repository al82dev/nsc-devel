# Tanda NSC 3.0.5 M02P — locales vivos de `$poll` MIR iterativos

Fecha: 2026-10-18
Rama: `development`

## SCOPE LOCK

M02P migra exclusivamente el DFS de `collectLivePollLocals`. El cambio es un refactor interno
sin cambio funcional: conserva preorden, el predicado `liveAcrossBoundary`, la deduplicación por
ordinal, los contenedores recibidos y los slots nulos.

No modifica la recolección de definiciones/usos/suspensiones, fuentes de borrow, clausura
transitiva, validación de fuentes no locales, publicación/layout del frame, MIR producido,
sintaxis, diagnósticos, API pública, ABI, NSMETA, LLVM ni caché. Quedan protegidos R02B5A y
`tests/unit/ProjectMethodBodyModelTests.cpp`. No se ejecutará baseline ni batería completa.

## Call graph y contrato observable

`lowerFunctions` llama `lowerOneFunction`; para `$poll`, `buildPollFrame` calcula primero los
mapas de definición y último uso y las posiciones `SuspendPoint`/`AsyncYieldValue`. Después de
recolectar fuentes de borrow llama `collectLivePollLocals`. El conjunto resultante se amplía con
la clausura de fuentes locales y finalmente controla qué declaraciones se publican en el frame.

El contrato histórico exacto es:

- recorrido preorden padre→hijos, de izquierda a derecha;
- un nodo o slot nulo se omite;
- sólo participa `LocalDeclaration` con `localOrdinal`;
- `liveAcrossBoundary` exige que existan definición y último uso;
- una suspensión cuenta sólo si `definition < boundary < lastUse`; la igualdad en cualquiera de
  los extremos no cuenta;
- basta la primera boundary que cumpla el predicado;
- el ordinal vivo se inserta en el set, por lo que declaraciones repetidas se deduplican;
- el set recibido no se limpia y conserva entradas preexistentes.

El collector no emite diagnósticos ni altera los mapas o boundaries. Cambiar el conjunto podría
omitir storage necesario o inflar el frame, y afectar la clausura de borrows y el layout async.

## Algoritmo

La implementación usa un worklist LIFO de punteros no propietarios. Inserta hijos en orden
inverso para conservar al extraerlos el preorden izquierda→derecha del DFS anterior. El
predicado `liveAcrossBoundary` permanece como autoridad única y sin modificaciones.

La complejidad sigue siendo `O(N·B)` en el peor caso para `N` nodos y `B` boundaries, porque esta
hoja no cambia el algoritmo del predicado. La pila nativa `O(H)` se sustituye por un worklist en
heap, `O(1)` adicional para una cadena y hasta `O(N)` para un árbol ancho.

## Contrafactual profundo

El focus `mir-live-poll-local-depth` construye una cadena de 32,768 declaraciones con un slot
nulo por nivel e intercala:

- ordinal 7, vivo porque `0 < 5 < 10`, repetido para demostrar deduplicación;
- ordinal 8, no vivo porque definición y boundary son iguales;
- ordinal 9, sin último uso;
- ordinal 10, cuyo último uso antecede la boundary;
- declaraciones sin ordinal.

El set se inicializa con el ordinal 42 para demostrar que no se limpia. La prueba exige
exactamente `{7, 42}` y destruye el árbol iterativamente para no medir la recursión de ownership
reservada para M02V.

## Infraestructura y validación

La prueba reutiliza `nsc_project_oop_lowering_tests`, `nsc.project-oop-lowering`, su selector por
variable de entorno y la ruta default. No agrega fuente, target, catálogo, manifiesto ni caso
Builder. `MirFunctionLowerer.cpp` permanece registrado exactamente una vez en CMake raíz y una
vez en compiler-suite.

Los gates se ejecutaron con `TEMP`/`TMP` en `A:\temp`. El proceso reconstruyó el `PATH` a
partir de sus valores nativos Machine y User; `where nslink.exe` resolvió exactamente
`A:\nslink-devel\bin\release\nslink.exe`, pero los consumidores lo invocaron exclusivamente por
el nombre `nslink.exe`.

- build `nsc`: PASS; contrato de host runtime estático: PASS;
- build `nsc_project_oop_lowering_tests`: PASS;
- build `nsc_component_tests`: PASS;
- focus `mir-live-poll-local-depth`: PASS 2/2;
- CTest `nsc.project-oop-lowering`: PASS 1/1;
- TL-LANG-370/371/373/374/434/435: PASS individual, salida 0 en los seis casos;
- `validate_source_registration.py`: PASS, salida 0.

La evidencia quedó en:

- `A:\temp\m02p-build-nsc-current.log`;
- `A:\temp\m02p-build-lowering-current.log`;
- `A:\temp\m02p-build-component-tests.log`;
- `A:\temp\m02p-focus-live-poll-local-depth-current.log`;
- `A:\temp\m02p-ctest-project-oop-lowering-current.log`;
- `A:\temp\m02p-tl-lang-370.log` … `A:\temp\m02p-tl-lang-435.log`;
- `A:\temp\m02p-source-registration.log`.

Estos resultados verifican exclusivamente la equivalencia de la recolección de locales vivos
de `$poll` y sus consumidores focales. No se ejecutó baseline ni batería completa.
