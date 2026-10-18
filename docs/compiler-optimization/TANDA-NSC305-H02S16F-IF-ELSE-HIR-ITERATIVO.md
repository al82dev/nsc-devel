# H02S16F · `if`/`else` integrado en la máquina HIR iterativa

## SCOPE LOCK

- Objetivo: eliminar las reentradas C++ de `IfStatement` hacia condición, bloque
  `then` y cadena `else if` mediante la worklist HIR común.
- Naturaleza: refactor interno; no cambia sintaxis, semántica, API, ABI, NSMETA,
  shape HIR, diagnósticos, rangos, cleanup ni política `noheap`.
- Archivos de producto: modelo HIR privado y máquinas HIR de expresión/statement.
- Evidencia: fixture focal existente de lowering OOP y este expediente.
- Protegido: Parser, Bodies, MIR, LLVM, Builder, instalación y tracking global.
- Validación autorizada: target `nsc_project_oop_lowering_tests` y selector único
  `hir-if-statement-depth`.
- Prohibido: CTest, Host completo, minisuite, batería, baseline, deploy y red.

## Contrato histórico congelado

El HIR de un `if` conserva:

1. nodo `If`, `builtin:void`, con la sintaxis original;
2. condición en contexto de valor como `children[0]`;
3. bloque `then` como `children[1]`;
4. bloque `else` o nodo `If` de `else if` como `children[2]`, si existe;
5. publicación transaccional: ningún nodo parcial escapa al fallar un hijo.

También se conserva una precedencia poco obvia pero observable del lowering
anterior: la condición se intenta primero y el bloque `then` se intenta incluso
si aquélla falla. Si ambos fallan, el texto posterior del `then` prevalece, pero
el primer rango válido puede seguir siendo el de la condición. El `else` no se
intenta cuando condición o `then` fallan.

## Diseño implementado

`lowerStatement` delega `IfStatement` a `lowerIfStatement`, que siembra una
continuación `IfStatement` en `lowerExpressionWithStatementFrame`. El mismo
frame se usa desde la raíz, desde `LoopBlock`, desde declaraciones sintéticas de
`Match` y para cadenas `else if`.

Las fases son:

- `0`: condición;
- `1`: bloque `then`;
- `2`: alternativa opcional;
- publicación: mover los hijos al nodo provisional y retirar el frame.

La condición entra por `lowerExpressionImpl` y recibe
`applyExpressionBoundaryPostconditions` una sola vez. El epílogo exterior omite
una segunda evaluación de política sobre el wrapper statement.

## Continuación iterativa de fallos

La implementación inicial de la tanda fue rechazada durante la auditoría porque
un scope guard volvía a llamar `lowerBlock` cuando fallaba la condición. Aunque
preservaba el texto del error somero, reintroducía recursión en cadenas de fallos.

La versión aceptada conserva los fallos dentro de la misma worklist:

1. el scheduler compartido se ejecuta bajo una frontera que convierte cualquiera
   de sus salidas tempranas en un resultado recuperable;
2. `resumePendingIfAfterFailure` localiza el `IfStatement` pendiente más cercano
   cuya condición estaba activa;
3. desmonta únicamente frames descendientes y restaura su profundidad cleanup;
4. guarda el texto de condición y conserva el primer `lastErrorRange_` válido;
5. programa el bloque `then` en la misma worklist, sin reentrada;
6. si `then` pasa, restaura el error de condición y propaga el fallo;
7. si `then` falla, conserva su texto posterior y el primer rango válido.

Sin un `if` pendiente, la frontera devuelve exactamente por la misma ruta de
error histórica. La ruta caliente de éxito sólo añade una comprobación de
resultado del scheduler; no clona AST/HIR ni crea una segunda worklist.

## Complejidad

- visita normal: `O(N)` tiempo;
- memoria explícita: `O(D)` para profundidad `D`;
- pila C++ para condición/then/else-if: `O(1)`;
- unwind de fallo: cada frame descendiente se descarta una vez;
- construcción y destrucción: ownership único, sin publicación parcial.

## Prueba focal

El selector `hir-if-statement-depth` cubre en una sola ejecución:

- 32,768 aristas alternadas condición `LoopExpression`, bloque `then` y
  `else if`, con shape/syntax/orden exactos;
- `if` con `else` bloque;
- bloque `then` ausente;
- condición fallida con `then` válido y `else` deliberadamente inválido para
  demostrar que el `else` no se visita;
- fallo exclusivo de `then`;
- doble fallo condición/then, texto posterior y primer rango;
- 32,768 condiciones fallidas encadenadas, que prueban unwind iterativo y
  descartan el scope guard recursivo.

## Evidencia

- Target exacto: `nsc_project_oop_lowering_tests`, PASS.
- Selector único: `hir-if-statement-depth`, PASS 10/10, exit 0.
- Build aislado: `A:\NSC-devel-paralelo\development-agente-1-hir\build-clang-cl-s16f`.
- No se ejecutaron pruebas adicionales, CTest, suites, baseline, instalación,
  deploy ni red.

