# H02S16G · `while`/`do` integrados en la máquina HIR iterativa

## SCOPE LOCK

- Objetivo: eliminar las reentradas C++ de `WhileStatement` y
  `DoWhileStatement` hacia condición, bloque y ciclos anidados mediante la
  worklist HIR común.
- Naturaleza: refactor interno; no cambia sintaxis, semántica, API, ABI,
  NSMETA, shape HIR, diagnósticos, rangos, cleanup ni política `noheap`.
- Archivos de producto: modelo HIR privado y máquinas HIR de
  expresión/statement.
- Evidencia: fixture focal de lowering OOP y este expediente.
- Protegido: Parser, Bodies, MIR, LLVM, Builder, instalación y tracking global.
- Validación autorizada: target `nsc_project_oop_lowering_tests` y selector
  único `hir-loop-statement-depth`.
- Prohibido: CTest, Host completo, minisuite, batería, baseline, deploy y red.

## Contrato histórico congelado

El lowering conserva para ambos statements un nodo `While` o `DoWhile`,
`builtin:void`, con la sintaxis original y exactamente dos hijos: condición en
`children[0]` y bloque en `children[1]`. El orden HIR histórico de `do` también
es condición seguida por cuerpo; esta tanda no reinterpreta ese contrato como
orden de ejecución.

La implementación anterior intentaba bajar primero la condición y después el
cuerpo aun cuando la condición había fallado. Si fallaban ambos, el texto del
cuerpo prevalecía y el primer rango válido podía seguir perteneciendo a la
condición. Esa precedencia observable también queda preservada.

## Diseño implementado

`lowerStatement` delega ambos kinds a `lowerLoopStatement`, que siembra un frame
`LoopStatement` en `lowerExpressionWithStatementFrame`. El mismo frame se
programa desde la raíz, desde `LoopBlock`, desde alternativas de `if` y desde
declaraciones sintéticas de `match`.

Las fases son:

- `0`: bajar condición en contexto de valor y aplicar una sola vez sus
  postcondiciones de frontera;
- `1`: bajar el cuerpo como `LoopBlock`;
- publicación: mover condición y cuerpo al nodo provisional y retirar el frame.

La continuación de fallos creada en H02S16F se generalizó para aceptar
`IfStatement` y `LoopStatement`. Ante una condición fallida, conserva el error
y el primer rango, desmonta sólo los frames descendientes y agenda el cuerpo en
la misma worklist. No llama recursivamente a `lowerBlock` ni crea una segunda
máquina.

## Cierres recursivos cubiertos

La prueba contrafactual profunda alterna dos SCC reales:

1. statement de bucle → cuerpo `LoopBlock` → statement de bucle;
2. statement de bucle → condición `LoopExpression` → `LoopBlock` → statement
   de bucle.

También construye 32,768 condiciones fallidas con cuerpos anidados para probar
que la recuperación completa permanece en la agenda explícita.

## Complejidad

- visita normal: `O(N)` tiempo;
- memoria explícita: `O(D)` para profundidad `D`;
- pila C++ para las aristas migradas: `O(1)`;
- cada frame se publica o descarta una vez;
- ownership único, sin HIR parcial publicado al fallar.

## Evidencia

- Target exacto: `nsc_project_oop_lowering_tests`, PASS.
- Selector único: `hir-loop-statement-depth`, PASS 8/8, exit 0.
- El target compiló `HirAggregateLowering.cpp` y confirmó que el bloqueo por
  parámetros sin usar ya está resuelto por el commit `4a82345f8`.
- Build aislado: `A:\nsc-devel\build-test-infrastructure`.
- No se ejecutaron pruebas adicionales, CTest, suites, baseline, instalación,
  deploy ni red.
