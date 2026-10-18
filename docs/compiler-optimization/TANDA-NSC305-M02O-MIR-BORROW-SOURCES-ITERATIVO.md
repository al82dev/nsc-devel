# Tanda NSC 3.0.5 M02O — fuentes de borrow MIR iterativas

Fecha: 2026-10-17
Rama: `development`

## SCOPE LOCK

M02O migra exclusivamente el DFS de `collectBorrowSources`. Es un refactor interno sin cambio
funcional: conserva la clasificación local/no local, el orden de sobrescritura por ordinal, la
selección exclusiva del primer initializer y el tratamiento de hijos nulos.

No modifica `collectLivePollLocals`, clausura transitiva de fuentes, validación de borrows no
locales, selección o layout del frame, MIR producido, sintaxis, diagnósticos, API pública, ABI,
NSMETA, LLVM ni caché. Quedan protegidos R02B4 y
`tests/unit/ProjectMethodBodyModelTests.cpp`. No se ejecutará baseline ni batería completa.

## Call graph y contrato observable

`ProjectOopMirModel::lowerFunctions` llama `lowerOneFunction`; para una función `$poll`,
`buildPollFrame` ejecuta, en este orden:

1. suma el tamaño de frames awaited;
2. calcula definiciones, últimos usos y suspensiones;
3. recolecta fuentes de borrows;
4. calcula locales vivos;
5. cierra transitivamente sus fuentes locales;
6. rechaza una fuente no local viva a través de suspensión;
7. publica locales y parámetros del frame.

El collector histórico aplica exactamente estas reglas:

- recorrido preorden padre→hijos, de izquierda a derecha;
- un nodo o slot nulo se omite;
- sólo participa una `LocalDeclaration` con ordinal y al menos un hijo;
- sólo `children[0]` es initializer, aunque otro hijo sea `BorrowValue`;
- si el initializer es `BorrowValue` con `localOrdinal`, el mapa del local declarado se
  sobrescribe; por ello gana la última declaración visitada en preorden;
- si el initializer es `BorrowValue` sin `localOrdinal`, el ordinal declarado se inserta en el
  conjunto de fuentes no locales;
- una clasificación posterior no borra entradas previas del otro contenedor;
- initializer ausente, nulo o de otra clase no muta los contenedores;
- los contenedores recibidos no se limpian ni reordenan.

La función no emite diagnósticos. Su salida alimenta tanto la clausura del frame como el
diagnóstico causal de borrow de parámetro o temporal a través de una suspensión.

## Algoritmo

La implementación usa un worklist LIFO de punteros no propietarios. Los hijos se insertan en
orden inverso para recuperar al extraerlos el mismo preorden izquierda→derecha. La evaluación
del primer initializer ocurre antes de insertar hijos, igual que en el DFS histórico.

El costo temporal permanece `O(N)` para `N` nodos. La pila nativa `O(H)` del DFS se sustituye por
memoria heap del worklist: `O(1)` adicional en una cadena y hasta `O(N)` en un árbol ancho.
Ningún nodo se clona y no cambia el ownership del MIR.

## Contrafactual profundo

El focus `mir-borrow-source-depth` construye 32,768 declaraciones anidadas. Intercala:

- fuentes locales repetidas para el ordinal 7, cuya última sobrescritura esperada es 132,767;
- una fuente no local repetida para el ordinal 8;
- ordinal 9 con initializer no borrow y un `BorrowValue` posterior que debe ignorarse;
- declaraciones sin ordinal;
- ordinal 10 con primer initializer nulo;
- un slot nulo adicional por nivel.

La prueba exige mapa y conjunto exactos y destruye el árbol iterativamente, de modo que mide el
collector y no la recursión de destrucción reservada para M02V.

## Infraestructura y validación

La prueba reutiliza `nsc_project_oop_lowering_tests`, su `add_test`
`nsc.project-oop-lowering`, el selector por variable de entorno y la ruta default. No agrega
fuentes, targets, catálogo, manifiesto ni caso Builder. `MirFunctionLowerer.cpp` permanece
registrado exactamente una vez en CMake raíz y una vez en compiler-suite.

La validación focal se ejecutó con `TEMP`/`TMP` y logs persistentes en `A:\temp`:

- build `nsc`: PASS y contrato de host runtime estático PASS (`m02o-build-nsc.log`);
- build `nsc_project_oop_lowering_tests`: PASS (`m02o-build-lowering.log`);
- focus `mir-borrow-source-depth`: PASS 2/2 (`m02o-focus-borrow-source-depth.log`);
- CTest `nsc.project-oop-lowering`: PASS 1/1 en 5.88 s, con
  `NSC_LINKER=nslink.exe` por el `PATH` nativo (`m02o-ctest-project-oop-lowering.log`);
- runner `nsc_component_tests`: target vigente PASS incremental, con `nsc.exe` del build actual
  y `NSC_NSLINK_EXECUTABLE:STRING=nslink.exe` (`m02o-build-component-tests.log`);
- TL-LANG-374, borrow de local preservado en frame: PASS, exit 0
  (`m02o-tl-lang-374.log`);
- TL-LANG-434, fuente no local rechazada causalmente: PASS, exit 0
  (`m02o-tl-lang-434.log`);
- `validate_source_registration.py`: PASS, exit 0 (`m02o-source-registration.log`).

No se ejecutaron baseline ni batería completa.
