# TANDA NSC 3.0.5 — H02S29: `hir::HirBuilder` iterativo

## Estado

- Tanda: `H02S29`.
- Rama autorizada: `development`.
- Revisión inicial auditada: `dc21f2d4b0127305a6e7a7c085a112a13e479f39`.
- Naturaleza: refactor interno no funcional.
- Validación de ejecución: **DIFERIDA A LA TANDA INTEGRAL por orden expresa del usuario**.

## SCOPE LOCK

### Objetivo exacto

Eliminar la recursión de proceso del recorrido AST→HIR efectuado por
`nativesharp::hir::HirBuilder` y sustituirla por frames/worklists explícitos, sin alterar el HIR
observable ni sus contratos.

### Ruta causal autorizada

1. Entrada: árboles AST con anidamiento profundo en declaraciones, bloques, sentencias,
   expresiones, direcciones y conversiones.
2. Operación: `HirBuilder::build` despacha a `lowerFunction`, `lowerBlock`, `lowerStatement`,
   `lowerExpression`, `lowerAddress` y `lowerConvertedOperand`.
3. Invariante vulnerado: la profundidad válida del AST no debe quedar limitada por la pila del
   proceso anfitrión.
4. Causa: el SCC de llamadas de lowering conserva un frame C++ por cada nivel del AST.
5. Corrección autorizada: una máquina iterativa con pila explícita que conserve exactamente el
   orden de evaluación, creación, enlace y diagnóstico.

### Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | HIR base |
| Producto | `src/HIR/HirBuilder.cpp` |
| Prueba durable | `tests/unit/HirTests.cpp`, sólo si resulta necesaria para cubrir el contrato |
| Expediente | este documento |
| Símbolos | implementación interna de `hir::HirBuilder`; API pública intacta |
| Configuración | O0; ejecución diferida |

Todo archivo y símbolo no enumerado queda protegido. En particular se preserva sin tocar el
cambio ajeno de `tests/unit/ProjectMethodBodyModelTests.cpp`.

### Contratos protegidos

- sintaxis, Sema y resolución de símbolos;
- forma, tipo, orden y ownership de todos los nodos HIR;
- orden izquierda→derecha de operandos, argumentos, índices y sentencias;
- precedencia de errores y texto exacto de diagnósticos de `HirBuilder`;
- API, ABI, NSMETA, formatos, artefactos y consumidores certificados en 3.0.4;
- semántica de lvalue/rvalue, conversiones, offsets, bounds checks y operaciones.

### Acciones prohibidas

- modificar parser, Sema, MIR, LLVM, driver, ABI o metadata;
- reactivar versionamiento NSMETA;
- introducir recursión en la nueva ruta;
- baseline, batería completa, deploy, instalación, push, pull o fetch;
- usar rutas, temporales, cachés, logs o artefactos fuera de `A:`;
- ejecutar compilaciones o pruebas en esta tanda.

### Criterio exacto de terminación

1. No queda una arista recursiva de lowering en `HirBuilder.cpp`.
2. Un único motor iterativo cubre declaraciones, bloques, sentencias, expresiones, direcciones y
   conversiones conservando orden y diagnósticos.
3. El diferencial contiene únicamente producto, cobertura durable indispensable y expediente.
4. El análisis estático y el gate de diferencial no encuentran expansión de alcance.
5. El commit local aislado queda creado y la validación pendiente se declara sin presentar éxito
   de build o pruebas.

## Evidencia inicial

La implementación inicial contiene las siguientes aristas recursivas reales:

- `lowerFunction → lowerBlock`;
- `lowerBlock → lowerStatement → lowerBlock/lowerStatement`;
- `lowerStatement → lowerExpression/lowerConvertedOperand`;
- `lowerExpression → lowerExpression/lowerAddress/lowerConvertedOperand`;
- `lowerAddress → lowerExpression/lowerAddress`;
- `lowerConvertedOperand → lowerExpression`.

Los bucles planos de miembros, parámetros, argumentos e índices no eliminan esas aristas: cada
descenso conserva frames nativos hasta completar el hijo. La prueba roja dinámica no se ejecuta
en este corte por la orden vigente; la evidencia de ejecución profunda queda para la tanda
integral.

## Diseño de la migración

### Motor único

`IterativeHirLowering` conserva una pila `std::vector<LoweringFrame>` y la reutiliza durante todo
el `CompilationUnit`. Cada frame registra:

- familia pendiente (`Global`, `Function`, `Block`, `Statement`, `Expression`, `Address` o
  `ConvertedOperand`, además de declaraciones planas);
- nodo AST, owner de conversión e índice de operando;
- estado de continuación e índice de hijo;
- valores HIR parciales;
- `ExpressionInfo` y tipo dimensional cuando la ruta los necesita.

`run` sólo opera sobre `frames.back()`. Descender agrega un frame; terminarlo lo retira y entrega
su resultado al frame padre. Ninguna profundidad del AST agrega frames a la pila C++.

### Orden preservado

La máquina reproduce expresamente el orden anterior:

1. declaraciones y sentencias en orden fuente;
2. condición, rama verdadera y rama falsa;
3. izquierda antes que derecha;
4. dirección antes que valor de una asignación;
5. argumentos e índices de izquierda a derecha;
6. base antes que miembro o índice;
7. conversiones en el orden registrado por `SemanticModel`.

También conserva las rutas donde el código anterior evaluaba un hermano posterior aunque un hijo
anterior devolviera `nullptr` (`if`, `while`, `for`, binarios y condicionales). Esto mantiene la
precedencia del primer diagnóstico almacenado, no sólo el árbol producido en casos verdes.

### Momento de materialización

El parent HIR se crea en el mismo momento relativo que antes:

- `Block`, declaraciones, `Call`, `Allocation`, `Deallocation` y statements se crean antes de
  sus hijos cuando así lo hacía la implementación original;
- unary/binary/conditional, stores, extracción de campo y direcciones se crean después de bajar
  los operandos correspondientes;
- `Load` sigue envolviendo una dirección sólo si `loadLValue` es verdadero;
- `Conversion` sigue envolviendo secuencialmente el valor ya bajado.

Esto preserva forma, enlaces `first/second/third/children`, símbolos, tipos, offsets, constantes,
operaciones, conversiones, flags de array, bounds checks y orden observable del HIR.

### Cobertura durable

El foco `NSC_HIR_TEST_FOCUS=hir-builder-depth` construye directamente, sin parser ni binder, un
`CompilationUnit` con una función y 32 768 bloques AST anidados. Enlaza únicamente el símbolo de
función indispensable, invoca `HirBuilder` y recorre iterativamente el HIR para exigir que los
32 768 bloques sobrevivan en el mismo orden. La prueba queda persistida pero no fue ejecutada en
esta tanda.

## Auditoría estática del diferencial

- El conjunto único de mensajes enviados a `fail(...)` es idéntico al de la revisión inicial;
  la comparación determinista no produjo altas ni bajas.
- La búsqueda de llamadas `lowerBlock/lowerStatement/lowerExpression/lowerAddress/
  lowerConvertedOperand` no encontró aristas entre frames: sólo declaraciones de adaptadores y
  delegaciones de entrada al motor iterativo.
- `git diff --check` terminó sin salida.
- Líneas agregadas de más de 120 columnas: `0`.
- Marcadores `TODO`/`FIXME` agregados: `0`.
- `tests/unit/ProjectMethodBodyModelTests.cpp` permanece ajeno a H02S29 y no se modifica, stagea
  ni incluye en su commit.
- No se modifican headers públicos, parser, Sema, MIR, LLVM, driver, ABI ni NSMETA.

## Validación

**VALIDACIÓN DIFERIDA A LA TANDA INTEGRAL.** En esta tanda no se ejecutan build ni pruebas. Esto
no constituye evidencia de compilación o funcionamiento.

Validación focal que deberá ejecutar la tanda integral:

1. construir `nsc_hir_tests` en el build O0 autorizado de `A:`;
2. ejecutar con `NSC_HIR_TEST_FOCUS=hir-builder-depth` y exigir `PASS 2/2`, exit `0`;
3. ejecutar `nsc_hir_tests` sin foco para comprobar las rutas HIR base existentes;
4. construir el target de producto `nsc` y exigir su contrato estático habitual.

La ausencia de ejecución actual se registra como evidencia faltante deliberada, no como PASS.
