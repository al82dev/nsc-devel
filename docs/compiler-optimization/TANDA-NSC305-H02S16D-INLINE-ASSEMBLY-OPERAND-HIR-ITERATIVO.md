# TANDA NSC 3.0.5 — H02S16D: operandos de inline assembly iterativos

## Alcance cerrado

H02S16D elimina exclusivamente la reentrada C++
`InlineAssemblyStatement -> lowerExpression(operand)` dentro de Stage 34 HIR.
La continuación de ensamblador vive ahora en la misma worklist que las expresiones,
los bloques de `loop` y las declaraciones de brazos `match`.

No se modificaron parser, análisis semántico, MIR, LLVM, ABI, API, NSMETA ni las
familias H02S16E y posteriores.

## Causa

El recorrido histórico de records era lineal, pero cada input/output abría otra
invocación pública de `lowerExpression`. Una cadena
`LoopExpression -> Block -> InlineAssemblyStatement -> operand LoopExpression`
retenía un frame C++ por nivel aun cuando cada invocación individual ya usara una
worklist.

## Diseño implementado

- `InlineAssemblyStatement` es un frame de la máquina común.
- El frame conserva el parent provisional, el índice monotónico del siguiente
  record y el operand pendiente.
- Líneas y clobbers se materializan localmente, sin llamar lowerers.
- Cada operand se agenda en la worklist activa con el contexto histórico exacto:
  `output || (input && constraint == "m")`.
- Al regresar el child se aplica una sola vez su frontera de políticas/noheap;
  sólo después se consulta el type binding y se publica el record en el parent
  provisional.
- Root statement, `LoopBlock` y declaraciones `Match` siembran el mismo protocolo.
- El parent se mueve al contenedor sólo tras consumir todos los records; cualquier
  fallo destruye el prefijo provisional.

## Contratos preservados

- bytes de `statement.text()`;
- orden e identidad source de todos los records;
- texto/metadata de líneas y clobbers;
- constraint, ordinal, flag y typeKey de inputs/outputs;
- valor para `r`, fixed-register e `i`;
- dirección para input `m` y todo output;
- una sola evaluación de outputs read-write;
- lookup de tipo posterior al lowering exitoso;
- primer diagnóstico y rango interior;
- cleanup y ownership sin regiones ni transfers inventados.

## Prueba focal registrada

Selector único:

```text
hir-inline-assembly-operand-depth
```

El selector incluye:

- contrafactual de 32,768 niveles que atraviesa realmente
  `loop -> block -> asm -> operand -> loop`;
- oráculo estructural completo por nivel y destrucción HIR iterativa;
- root vacío de finalización síncrona;
- continuidad asm dentro de declaraciones `Match`;
- inputs `r`, fixed-register, `i`, `m`;
- outputs `=r`, `+r`, `=m`, `+m` y fixed-register;
- orden, bytes, source, línea y clobbers register/flags/memory;
- records nulos/desconocidos, operand ausente, error interno, type binding ausente
  y typeKey vacío;
- noheap observable en el segundo operand después de un primer operand válido;
- conservación de rango interior, cleanup activo y ownership.

## Validación autorizada

Sólo se autoriza:

1. compilar el target `nsc_project_oop_lowering_tests`;
2. ejecutar ese binario con
   `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-inline-assembly-operand-depth`.

No se ejecutan CTest, suites, minibaseline ni baseline.

## Evidencia

La revisión independiente estática de producto, fixture y dispatcher concluyó
`PASS` antes de la validación. Confirmó que no queda reentrada propia en la ruta
ejercitada: root statement, `LoopBlock` y `Match` siembran el mismo frame; el
operand leaf usa `lowerExpressionImpl`; el finalizador no invoca lowerers.

Compilación exacta:

```text
cmake --build A:\NSC-devel-paralelo\builds\agente-1-s14a \
  --target nsc_project_oop_lowering_tests
```

Resultado: `PASS`, exit code `0`.

- log: `A:\NSC-devel-paralelo\logs\agente-1\h02s16d-build-final.log`;
- SHA-256: `2A33406D40768EB8EDBD37CE459E423148DFB201535C6443C04003401E0C4A23`.

Ejecución exacta:

```text
NSC_PROJECT_OOP_LOWERING_FOCUS=hir-inline-assembly-operand-depth \
  A:\NSC-devel-paralelo\builds\agente-1-s14a\tests\nsc_project_oop_lowering_tests.exe
```

Resultado: `PASS: 15 checks`, exit code `0`.

- log: `A:\NSC-devel-paralelo\logs\agente-1\h02s16d-focus-final.log`;
- SHA-256: `740FC906A2EF75394CDBB2824F2F3338ABFBB2081E9E9031C81436C03BCB5F07`.

No se ejecutaron CTest, suites, minibaseline, baseline ni selectores adicionales.
