# TANDA NSC 3.0.5 — H02S30: `hir::HirVerifier` iterativo

## Estado

- Tanda: `H02S30`.
- Rama autorizada: `development`.
- Revisión inicial observada: `2f1a210c91166fe60b24a61f583c307149c8b42f`.
- Revisión base del commit: `22d4f9eaa`.
- Diferencial de `HirVerifier` entre ambas revisiones: vacío.
- Naturaleza: refactor interno no funcional.
- Validación de ejecución: **DIFERIDA A LA TANDA INTEGRAL por orden expresa del usuario**.

## SCOPE LOCK

### Objetivo exacto

Eliminar toda recursión de proceso del recorrido estructural de `nativesharp::hir::HirVerifier`
mediante una pila LIFO explícita, conservando exactamente el orden de visita, el primer diagnóstico,
los tipos, el contexto de función, la profundidad de bucle y los contratos congelados de 3.0.4.

### Ruta causal autorizada

1. Entrada: un `HirNode` raíz que puede contener árboles HIR de profundidad no acotada.
2. Operación: `HirVerifier::verifyNode` valida el nodo y desciende por `first`, `second`, `third` y
   `children`.
3. Invariante vulnerado: la validez de un HIR no debe depender de la capacidad de la pila nativa del
   proceso anfitrión.
4. Causa: cada arista descendente conservaba un frame C++ de `verifyNode` o de
   `verifyChildren`.
5. Corrección autorizada: representar los descensos y continuaciones con tareas explícitas en un
   `std::vector`, sin cambiar ninguna regla de validación.

### Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | Verificación HIR |
| Producto | `include/nativesharp/HIR/HirVerifier.h`, `src/HIR/HirVerifier.cpp` |
| Expediente | este documento |
| Símbolos | implementación privada de `hir::HirVerifier` |
| Configuración | O0; ejecución diferida |

Todo archivo y símbolo no enumerado queda protegido. En particular no se modifica, stagea ni
incluye el cambio ajeno de `tests/unit/ProjectMethodBodyModelTests.cpp` ni ningún frente MIR,
driver, expresión HIR o infraestructura concurrente.

### Contratos protegidos

- API pública de `HirVerifier` y ABI de NSC;
- conjunto y texto exacto de todos los diagnósticos;
- regla de conservar el primer error almacenado;
- orden DFS izquierda→derecha;
- momento relativo de validaciones estructurales y validaciones posteriores al hijo;
- propagación exacta de `functionReturnType` y `loopDepth`;
- identidad de tipos, símbolos, offsets, operaciones y relaciones de puntero;
- sintaxis, Sema, MIR, LLVM, CLI, metadata, NSMETA, artefactos y consumidores 3.0.4;
- comportamiento de O0, O1, O2 y O3.

### Acciones prohibidas

- agregar, retirar o relajar validaciones HIR;
- cambiar mensajes, precedencia de errores o tipos aceptados;
- modificar pruebas, fixtures, parser, Sema, MIR, LLVM, driver, ABI o metadata;
- reactivar versionamiento NSMETA;
- introducir recursión, callbacks recursivos o estado global mutable;
- ejecutar compilaciones, pruebas, baseline, deploy o instalación;
- usar otras unidades, MSYS, workspaces auxiliares, `push`, `pull` o `fetch`.

### Criterio exacto de terminación

1. `verifyNode` no se llama a sí mismo directa ni indirectamente.
2. `verifyChildren` recursivo deja de existir.
3. Los 35 `HirNodeKind` conservan su validación y su orden observable.
4. Los 63 literales de diagnóstico permanecen idénticos como conjunto.
5. El diferencial pasa los gates estáticos autorizados.
6. Código y expediente quedan en un commit local aislado.
7. Build y pruebas se declaran pendientes, nunca como aprobados.

## Evidencia inicial

### Aristas recursivas encontradas

La revisión base contenía 38 apariciones de `verifyNode(`: una definición, una llamada de entrada y
36 apariciones adicionales que formaban descensos recursivos. El SCC abarcaba:

- `CompilationUnit`, `StructDeclaration`, `EnumDeclaration` y `Block` mediante
  `verifyChildren → verifyNode`;
- inicializadores de variables y cuerpos de función;
- condición, cuerpo y alternativa de `If`;
- condición y cuerpo de `While`;
- inicializador, condición, incremento y cuerpo de `For`;
- valor de `Return` y operandos de `ExpressionStatement`;
- `Load`, `Store`, `Conversion`, `Unary`, `Binary` y `Conditional`;
- argumentos de `Call`;
- base e índice de `IndexAddress`;
- `FieldAddress`, `DereferenceAddress` y `ExtractField`;
- `CompoundAssignment` e `Increment`;
- cuenta de `Allocation`/`StackAllocation` y operando de `Deallocation`.

La profundidad máxima de esa cadena coincidía con la profundidad del HIR. Un árbol válido profundo
podía, por tanto, desbordar la pila nativa antes de terminar la verificación.

## Diseño implementado

### Pila explícita

`VerificationTask` representa una unidad pendiente con:

- clase de tarea;
- nodo HIR;
- tipo de retorno de la función activa;
- profundidad de bucle;
- índice de hijo para llamadas.

`verifyNode` mantiene un `std::vector<VerificationTask>` y procesa siempre `back()`. Descender
agrega una tarea; terminar un nodo elimina su tarea. La profundidad HIR consume almacenamiento de
heap controlado por el vector y ya no agrega frames C++.

### Clases de continuación

La tarea `Node` ejecuta la validación estructural que antes precedía a cada descenso. Cuatro
continuaciones conservan puntos donde no era válido adelantar una comprobación:

1. `VariableInitializerType`: compara el tipo del inicializador sólo después de verificar su
   subárbol. Así, un error interno del inicializador sigue precediendo al error de tipo del padre.
2. `IfElseBranch`: inspecciona la forma del `else` sólo después de verificar condición y cuerpo,
   exactamente como el código anterior.
3. `ReturnType`: compara el tipo del valor y del nodo `Return` después de verificar el valor.
4. `CallArgument`: valida forma y tipo de un argumento, verifica todo su subárbol y sólo entonces
   avanza al argumento siguiente.

Estas continuaciones evitan convertir la migración en un preescaneo que cambiaría el primer
diagnóstico visible.

### Orden de recorrido preservado

| Familia | Orden anterior y actual |
|---|---|
| listas de declaraciones/sentencias | índice `0..N-1` |
| `If` | condición, cuerpo, validar forma de `else`, `else` |
| `While` | condición, cuerpo con `loopDepth + 1` |
| `For` | inicializador, condición, incremento, cuerpo con `loopDepth + 1` |
| binarios y stores | `first`, después `second` |
| condicional | condición, rama verdadera, rama falsa |
| llamadas | validar y recorrer cada argumento antes del siguiente |
| función | parámetros estructurales, cuerpo con retorno propio y profundidad cero |

Las listas se agregan a la pila en orden inverso para que la extracción LIFO reproduzca el orden
fuente. Ningún nodo, hermano o continuación se adelanta respecto del recorrido anterior.

### Contextos preservados

- `CompilationUnit` reinicia `functionReturnType = nullptr` y `loopDepth = 0`.
- `FunctionDeclaration` entra al cuerpo con el tipo de retorno resuelto y profundidad cero.
- `While` y el cuerpo de `For` incrementan únicamente la profundidad del cuerpo.
- inicializador, condición e incremento de `For` conservan la profundidad exterior.
- ramas de `If`, operandos y argumentos heredan sin cambios ambos contextos.

### API y ownership

La superficie pública sigue siendo:

```cpp
bool verify(const HirNode* root);
const std::string& error() const noexcept;
```

Se retira sólo el helper privado recursivo `verifyChildren`. La pila contiene punteros no
propietarios ya administrados por el HIR; no cambia lifetime, ownership ni mutabilidad del árbol.

## Auditoría estática del diferencial

- Casos `HirNodeKind` en la revisión base: `35`.
- Casos `HirNodeKind` en la implementación iterativa: `35`.
- Diferencias entre ambos conjuntos: `0`.
- Literales únicos en la revisión base: `63`.
- Literales únicos en la implementación iterativa: `63`.
- Diferencias entre ambos conjuntos: `0`.
- Apariciones de `verifyNode(` antes: `38`.
- Apariciones de `verifyNode(` después: `2` — definición y entrada pública.
- Apariciones restantes de `verifyChildren`: `0`.
- Aristas productivas recursivas restantes en `HirVerifier`: `0`.
- `git diff --check`: sin salida.
- Líneas agregadas mayores de 120 columnas: `0`.
- Tests, fixtures, contratos públicos, parser, Sema, MIR, LLVM, driver y NSMETA: sin cambios.

La segunda pasada revisó expresamente los cortocircuitos cuya precedencia podía alterarse:
inicializadores, cuerpo de función, `If`, `For`, retornos, expression statements, argumentos de
llamada, operaciones binarias e indexación. No se adelantó ninguna comprobación posterior a un
hijo.

## Validación

**VALIDACIÓN DIFERIDA A LA TANDA INTEGRAL.** En esta tanda no se ejecutaron build, análisis del
compilador ni pruebas. El cierre es estático y no constituye evidencia de compilación o ejecución.

La tanda integral deberá, como mínimo:

1. construir el target O0 `nsc_hir_verifier_tests` en un build autorizado bajo `A:`;
2. ejecutar `nsc_hir_verifier_tests` y exigir sus nueve expectativas actuales, exit `0`;
3. agregar o ejecutar un contrafactual directo con HIR válido de profundidad adversarial que
   demuestre que `HirVerifier::verify` ya no consume pila nativa por nivel;
4. comparar exactamente el primer diagnóstico contra la revisión congelada en árboles con dos
   fallos posibles en hermanos o en una continuación diferida;
5. construir el producto `nsc` y ejecutar el gate HIR que corresponda a la tanda integral.

Hasta que esos puntos se ejecuten, el estado correcto es:

`CIERRE ESTÁTICO COMPLETO; VALIDACIÓN DE BUILD PENDIENTE DEL USUARIO`.
