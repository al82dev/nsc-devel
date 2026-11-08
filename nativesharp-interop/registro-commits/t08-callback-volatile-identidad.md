# Calificador `volatile` en la firma de callbacks C

## Problema

Reproducción de partida: [t08-callback-volatile-invocacion.md](t08-callback-volatile-invocacion.md). Al invocar
un typedef de callback C con parámetro `volatile Payload*` mediante una plantilla precompilada, NSC rechazaba el
argumento con `indirect call cannot yet preserve pointer pointee volatile qualification`.

La causa es la identidad estructural de la función (`fn:`), que no conservaba el calificador. Las sondas de
`A:\nsc-interop-build\claude\sondas\ret` (O0, encabezado directo) mostraron que la misma causa tenía otras
manifestaciones, todas aceptadas en silencio antes de esta corrección:

| Código NativeSharp | Clang 23.1.0 | NSC antes |
|---|---|---|
| `GetFn g = get_payload; Payload* p = g();` (retorno `volatile Payload*`) | advierte que se descartan calificadores | aceptado sin diagnóstico |
| `PlainFn f = take_volatile;` | error `incompatible function pointer types` | aceptado |
| `TakeFn t = take_plain;` | error `incompatible function pointer types` | aceptado |
| `Payload* q = get_payload();` (llamada directa) | advierte | rechazado: `initialization would discard...` |

## Contrato

- La identidad `fn:` registra, por parámetro y para el resultado, si el puntero apunta a datos `volatile`.
- Se escribe como sufijo opcional `:volatile-pointee:<máscara>` después del tipo de error: un `0`/`1` por
  parámetro y uno final para el resultado. Solo aparece si hay al menos un `1`; las identidades sin el
  calificador conservan exactamente sus bytes (`TL-LANG-025` sigue válido). El analizador rechaza cualquier
  otro texto final, una máscara de longitud incorrecta o una máscara sin marcas.
- Asignación entre tipos `fn`: las marcas deben coincidir exactamente, como en C.
- Conversión de un método o función C a un callback estructural: el método debe tener exactamente la misma
  calificación en cada parámetro y en el retorno.
- Invocación: un argumento con dato apuntado `volatile` solo se acepta en un parámetro marcado; en un parámetro
  sin marca, en una posición variádica o sin información estructural se rechaza con
  `indirect call argument would discard pointer pointee volatile qualification`.
- El resultado de un callback con retorno `volatile T*` conserva la calificación, igual que una llamada directa.
- Inferencia `var f = funcion;`: el tipo natural `fn*` copia las marcas de la firma del método.
- Materialización de una firma de metadata como fuente NativeSharp: el lenguaje no puede escribir esa
  calificación, así que se rechaza explícitamente en lugar de descartarla.
- **Ausencia:** un programa sin punteros `volatile` en firmas de callback produce las mismas identidades,
  conversiones y código que antes.

## Archivos y símbolos

| Archivo | Cambio |
|---|---|
| `include/nativesharp/Language/FunctionType.h` | campos `parameterPointeeVolatile`, `resultPointeeVolatile` |
| `include/nativesharp/Language/CallableSemantics.h` | `functionParameterPointeeVolatile`, `functionHasPointeeVolatile` |
| `src/Language/CallableSemantics.cpp` | sufijo en `functionTypeIdentity`; lectura en `parseFunctionTypeIdentity`; comparación en `functionTypesAssignable` |
| `src/Frontend/MemberModel/TypeIdentity.cpp` | `canonicalFunctionTypeKey` toma las marcas de la sintaxis (typedef C) |
| `src/Frontend/Generics/TypeSpellingAndVisibility.cpp` | `functionTypeSpelling` igual que la clave canónica |
| `src/Frontend/Bodies/StatementAnalyzer.cpp` | `appendMethodPointeeVolatileFlags` para el tipo natural de `var` |
| `src/Frontend/Bodies/LambdaDelegateBinder.cpp` | `methodPointeeVolatileMatches`; filtro en `bindMethodGroupToDelegate` y `methodGroupExactlyMatchesDelegate` |
| `src/Frontend/Bodies/CallAnalyzer.cpp` | `bindDelegateInvocationArguments` comprueba la marca; `analyzeDelegateInvocationCall` propaga el resultado |
| `src/Driver/PrecompiledSurface/MetadataSourceMapping.cpp` | rechazo explícito de firmas con marcas |

Consumidores de la identidad revisados (leen la estructura con `parseFunctionTypeIdentity` y no reconstruyen la
cadena campo por campo): `ProjectTypeConversion`, `TypeKeyPredicates`, `ConversionAndPassKindChecks`,
`HirBuildCoordinator`, `BuildAndDeclaration`, `ProjectGenericConstraintModel`, `LibraryMetadataConstruction`,
`LibraryMetadataVerification`, `CppSemanticRecord`, `LlvmTypeClassification`, `PackedArrayAlignment` y los
`OopLlvm*` de delegates y thunks. `CppSemanticFunction` sigue rechazando punteros a función con parámetros
`volatile` (no produce marcas). No hay recursión nueva; todo es O(número de parámetros).

## Pruebas

- `typedef-calificadores/validar-invocacion.py` dejó de ser reproducción: ahora exige que la invocación del
  callback (dos DLL, plantillas precompiladas, consumidor sin encabezados, fallo y acierto de caché) compile y se
  ejecute. Matriz ejecutada: O0–O3 × {`volatile`, `const volatile`}: 8 de 8 correctas.
- `typedef-calificadores/validar-callback-volatile.py` (nuevo):
  - control C con Clang: valores esperados, rechazo de conversiones incompatibles y advertencia del retorno;
  - IR: la lectura `p->value` a través del resultado del callback tiene las mismas cargas volatile que la
    lectura directa;
  - seis rechazos (directo y precompilado sin encabezado; en el precompilado el diagnóstico sale al compilar el
    consumidor): argumento `volatile` a parámetro sin marca, conversión que quita o agrega la marca, retorno
    asignado a puntero sin `volatile`, asignación entre tipos `fn` distintos y argumento variádico;
  - seis casos positivos en O0–O3, directos y precompilados sin encabezado, ejecutados: invocación, retorno,
    `var` natural, variádico, argumento sin `volatile` a parámetro `volatile` y caso sin calificador.
- Regresiones dirigidas: `pruebas-tanda-08-callback` (validar y record precompilado), `enum-funcion` (validar y
  callback precompilado), `bool-callback-c`, `volatile-punteros-c`, `typedef-funcion-record` y
  `typedef-calificadores/validar.py` en O0 y O3. Resultados en la sección final.

Los casos positivos directos dependen de la corrección registrada en
[t08-callback-direccion-coordinador.md](t08-callback-direccion-coordinador.md).

## Pendientes derivados (no implementados)

- `const` del dato apuntado tampoco forma parte de la identidad (comentario existente en `CppSemanticFunction`).
- `CppSemanticFunction` no importa funciones, campos ni globales cuyo tipo es un puntero a función con
  parámetros `volatile T*`; solo el camino de typedef conserva la calificación.
- **INFERENCIA, no verificada:** una lambda o método NativeSharp convertido a un callback con parámetro
  `volatile T*` no marca su parámetro, de modo que su cuerpo accede sin `volatile`.
- Un `delegate` con nombre declarado con un alias C `volatile` no lleva la marca en su firma.
- Una firma pública con callback `volatile` no puede materializarse como fuente desde metadata (ahora con
  rechazo explícito).

## Resultados de regresión

Compilador: construcción incremental de `A:\nsc-interop-build\tanda1-ON` (build `0ea233537775f4aa`), que incluye
también la corrección de [t08-callback-direccion-coordinador.md](t08-callback-direccion-coordinador.md).

| Prueba | Resultado |
|---|---|
| `typedef-calificadores/validar-invocacion.py`, O0–O3 × {`volatile`, `const volatile`} | 8 de 8 correctas |
| `typedef-calificadores/validar-callback-volatile.py` (control C, IR, 6 rechazos, 6 positivos O0–O3) | correcto |
| `typedef-calificadores/validar.py` O0 y O3 | correcto |
| `typedef-funcion-record/validar.cmd` | correcto |
| `pruebas-tanda-08-callback/validar.cmd` (O0–O3, IR volatile) | correcto |
| `pruebas-tanda-08-enum-funcion/validar.cmd` y `validar-callback-precompilado.cmd` | correcto |
| `pruebas-tanda-08-bool-callback-c/validar.cmd` | correcto |
| `pruebas-tanda-08-volatile-punteros-c/validar.cmd` | correcto |
| `pruebas-tanda-08-callback/validar-record-precompilado.cmd` | **falla** (ver abajo) |

`validar-record-precompilado.cmd` falla al compilar `RecordConsumer.ns` con `conflicting C/C++ type alias
'RecordCallback' (projects 1,0; ...; reconciled=no)` aunque ambos tipos son idénticos. El diagnóstico sale de
`ProjectNameLookup::expandCppTypeAlias` (`QualifiedTypeNames.cpp`), que decide con datos del catálogo de Clang
(`canonicalSpelling`, `equivalentRecordFunctionAliases` de `CppSemanticInventory.cpp`), antes de construir
ninguna identidad `fn:`. **INFERENCIA por lectura de código, no verificada con un binario sin estos cambios:** el
fallo no proviene de esta corrección. Queda registrado como pendiente en el tracking (T08-017-02).

Las pruebas unitarias del compilador (`TL-LANG-025` a `TL-LANG-029`) no se ejecutaron en esta máquina; por
lectura, las identidades sin calificador no cambian y los casos inválidos siguen rechazándose.
