# Tanda NSC 3.0.5 H02S14K: core Console, Enum y Struct iterativo

## SCOPE LOCK

- Objetivo: eliminar únicamente las reentradas HIR de Console, Enum, Struct y
  `Environment.Exit` enumeradas por H02S14K.
- Base: `41bc2bc7e90211a358f5cb348ae72e028e96a7cb`.
- Producto autorizado: máquina HIR común y guards residuales de estas rutas.
- Infraestructura autorizada: acceso focal de modelos/bindings y selector
  `hir-console-enum-struct-call-depth`.
- Protegido: parser, análisis semántico, ABI, NSMETA, MIR, LLVM, runtime,
  trackings globales y hojas H02S14L.
- Validación única: target `nsc_project_oop_lowering_tests` y el selector focal.
- Prohibido: CTest, suites, minibaseline, baseline, deploy, instalación,
  `fetch`, `pull` y `push`.

## Causa

`ProjectOopHirModel::lowerCallExpression` todavía reentraba directamente a
`lowerExpression` para los argumentos de Console, el receptor de Enum, el
receptor transparente de `StructClone`, receptor/argumento de los agregados y
el argumento de `Environment.Exit`. Una cadena válida de 32,768 `Clone()` podía
agotar la pila nativa aunque la máquina HIR exterior ya fuera iterativa.

## Diseño

La máquina común reconoce seis formas explícitas:

- `ConsoleOutputCall`;
- `ConsoleReadLineCall`;
- `EnumToStringCall`;
- `StructCloneCall`;
- `StructAggregateCall`;
- `EnvironmentExitCall`.

Cada forma valida completamente antes de descender. El scheduler visita un hijo
a la vez y el finalizador consume sólo el HIR ya producido:

- Console conserva las máscaras 0..3, conversión opcional, borrow y cleanup de
  fresh-owned;
- ReadLine conserva un status sin wrappers y tipo fijo `builtin:string`;
- Enum congela enumeradores en orden declarado, baja una vez el receptor y
  conserva fallback, aliases y bits negativos;
- `StructClone` devuelve directamente el receptor, sin nodo adicional;
- Equals/Hash conservan receiver-first, `argumentCount` previo a los leaf
  descriptors y reutilizan `appendAggregateLeaves`;
- `Environment.Exit` conserva su conversión opcional sin borrow ni cleanup.

`StructToString` permanece como constante sin evaluar receptor. Las veinte hojas
Clock/Environment/Process/SystemInfo/Debug permanecen en `lowerCallExpression`,
sin hijos y sin ser desviadas por el nuevo clasificador. Los bloques residuales
migrados sólo conservan validaciones y guards de escape; no bajan hijos.

## Prueba focal

`hir-console-enum-struct-call-depth` cubre en una sola ejecución:

- 32,768 `StructClone` nominales y transparentes;
- cuatro máscaras Console, conversiones, call opcional, borrow ordinario y
  fresh-owned con cleanup;
- ReadLine y Environment.Exit;
- layout Enum con fallback, valor negativo y alias en orden declarado;
- StructToString, Clone, Equals y Hash;
- las veinte hojas core vecinas;
- diagnósticos previos de address, conteo, receptor, modelos y tipo.

La destrucción del producto profundo usa el helper iterativo existente.

## Validación autorizada

- Compilar únicamente `nsc_project_oop_lowering_tests`.
- Ejecutar únicamente `hir-console-enum-struct-call-depth`.
- La batería completa queda reservada para la GUI del usuario.

## Resultado

- El target único `nsc_project_oop_lowering_tests` compiló y enlazó.
- El selector único `hir-console-enum-struct-call-depth` terminó con
  `PASS: 51 checks`, código de salida 0.
- La auditoría estática confirmó cero llamadas a `lowerExpression` dentro de
  los guards residuales y los finalizadores H02S14K.
- Evidencia en A: `h02s14k-build-final.log`, `h02s14k-focus-final.log` y
  `h02s14k-static-audit.log` bajo
  `A:\NSC-devel-paralelo\logs\agente-1`.
