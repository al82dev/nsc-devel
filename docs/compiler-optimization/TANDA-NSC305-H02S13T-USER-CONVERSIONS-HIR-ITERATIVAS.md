# NSC 3.0.5 — H02S13T: conversiones de usuario HIR iterativas

## Alcance cerrado

Esta hoja elimina la reentrada recursiva de los `CastExpression` enlazados a miembros
`isUserConversion`. La clasificación, el descenso del operando y la terminación de la llamada
ocurren ahora dentro de la máquina iterativa común de expresiones.

No se modifican sintaxis, AST, selección de overload, ranking de conversiones, HIR público, MIR,
LLVM, ABI, NSMETA ni diagnósticos contractuales. Las demás familias de llamadas continúan
reservadas para H02S14.

## Causa y contrato preservado

La ruta anterior entraba en `lowerConversionExpression`, construía el vector de un operando y
llamaba a `lowerBoundCall`. Al no proporcionar un argumento ya bajado, `lowerBoundCall` volvía a
`lowerExpression`; una cadena de conversiones de usuario consumía una trama nativa por nivel.

El frame `UserConversion` conserva estos contratos:

- sólo clasifica un cast con binding resuelto, ordinal válido y miembro `isUserConversion`;
- congela el binding antes de descender;
- baja `first()` exactamente una vez con `addressContext = false`;
- entrega a `lowerBoundCall` un vector `preloweredArguments` de exactamente un elemento;
- deja en `lowerBoundCall` la aplicación de conversiones seleccionadas, borrow, cleanup,
  argumentos predeterminados, metadata y tratamiento de reference result;
- propaga el `addressContext` exterior únicamente a la terminación del resultado;
- conserva como conversiones builtin/runtime los bindings ausentes, incompletos o no válidos.

`lowerConversionExpression` conserva un rechazo defensivo si una conversión de usuario escapara
de la máquina. Así no queda una segunda ruta recursiva silenciosa.

## Algoritmo y complejidad

Cada cast agrega un frame a la worklist, visita su único operando y, durante el unwind, reutiliza
el ensamblador canónico de llamadas con el HIR ya producido. Para N conversiones anidadas, el
trabajo propio es O(N), los frames ocupan O(N) en heap y el stack nativo permanece O(1) para esta
arista.

## Prueba puntual

Selector único: `hir-user-conversion-depth`.

Sus seis comprobaciones cubren:

1. descubrimiento real de miembros `op_Implicit`, `op_Explicit` y caller;
2. una cadena alternada de 32,768 conversiones sin recursión del proceso;
3. orden, ordinal, tipo de resultado, aridad y leaf exactos;
4. conversión seleccionada no identidad, borrow temporal y cleanup aplicados una sola vez;
5. diagnóstico exacto para un argumento predeterminado sin parámetro correspondiente;
6. tratamiento exacto de reference result en contexto de valor y de dirección.

## Evidencia focal

- Build aislado: `A:\NSC-devel-paralelo\builds\agente-1-s13r`.
- Target único: `nsc_project_oop_lowering_tests` — PASS, exit code 0.
- Selector único: `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-user-conversion-depth` —
  `PASS: 6 checks`, exit code 0.
- Primer build del foco: detectó un alias de tipo faltante sólo en la prueba; se corrigió sin
  ampliar producto ni ejecutar otra prueba.
- Build final: cero warnings nuevos.
- Logs:
  - `A:\NSC-devel-paralelo\logs\agente-1\h02s13t-build-final2.log`
  - `A:\NSC-devel-paralelo\logs\agente-1\h02s13t-focus-final.log`

No se ejecutaron CTest, unidad completa, suite, minibaseline, baseline, deploy ni instalación.

## Límites y siguiente dependencia

La evidencia corresponde exclusivamente a H02S13T. La batería amplia permanece reservada para
la GUI del usuario al terminar todos los ítems. H02S14A queda habilitado como siguiente hoja HIR.
