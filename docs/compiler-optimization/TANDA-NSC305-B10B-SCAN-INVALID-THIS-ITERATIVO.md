# NSC 3.0.5 — B10B: `scanInvalidThisContexts` iterativo

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`, `f6829a92a`.
- **Entrada roja:** función procedural cuyo `return` contiene una llamada con receptor calificado
  de 16,384 `MemberExpression` y dos argumentos `this` inválidos.
- **Operación candidata:** `ProjectMemberModel::scanInvalidThisContexts`, que se auto-invoca por
  `first/second/third/children` al recorrer contextos ajenos a métodos de instancia.
- **Naturaleza:** refactor interno no funcional para retirar profundidad de pila nativa.
- **Archivo/símbolo autorizado:** `src/Frontend/ProjectMemberModel.cpp`, sólo
  `scanInvalidThisContexts`; `tests/unit/ProjectMemberModelTests.cpp`, expediente y tracking.
- **Orden congelado:** preorden `first`, `second`, `third`, `children`; corte completo de subárbol
  para `MethodDeclaration`, `ConstructorDeclaration` y `DestructorDeclaration`; diagnóstico y
  retorno temprano al encontrar `ThisExpression`.
- **Contratos congelados:** cantidad, identificador, texto, ubicación y orden de diagnósticos;
  sintaxis, semántica, API, ABI, NSMETA y artefactos.
- **Protegidos:** `scanMethodBody`, parser, generics, names, method-bodies, HIR, MIR, LLVM y
  consumidores.
- **Prohibido:** modificar cortes de contexto, aceptar `this` nuevo, mezclar otra tanda, reducir
  profundidad, baseline, suite completa, deploy, instalación o red.
- **Gate:** rojo propio con `0xC00000FD`; recorrido iterativo con el mismo preorden y los mismos
  cortes; dos diagnósticos exactos en orden fuente; target focal completo verde; build focal de
  `nsc`; diff y commit local aislado.

## Cadena causal a comprobar

```text
función procedural con receptor de 16,384 MemberExpression
→ ProjectMemberModel::build
→ scanInvalidThisContexts(unit->syntaxTree())
→ auto-llamada por first() una vez por segmento
→ 0xC00000FD
```

La declaración es procedural, no un método, constructor ni destructor. Por ello
`scanMethodBody` no recorre este cuerpo y el descenso profundo pertenece exclusivamente a B10B.

## Estado

Reproducción roja confirmada: el target focal terminó en `native-exit=-1073741571`
(`0xC00000FD`) al entrar en el caso profundo procedural.

## Implementación

`scanInvalidThisContexts` conserva una worklist LIFO de nodos. Cada visita descarta nulos,
mantiene el corte completo de declaraciones de método, constructor y destructor, y publica el
mismo `InvalidMemberAccess` antes de terminar la visita de un `ThisExpression`. Los hijos se
apilan en orden inverso (`children`, `third`, `second`, `first`) para consumir exactamente el
preorden anterior `first`, `second`, `third`, `children`.

No se modificaron `scanMethodBody`, parser, generics, names, method-bodies, HIR, MIR ni LLVM.

## Evidencia focal

- Rojo propio: `native-exit=-1073741571` (`0xC00000FD`).
- La prueba profunda procedural contiene 16,384 segmentos y dos `this` inválidos.
- Target `nsc_project_member_model_tests.exe`: `PASS: 74 checks`.
- Se comprobaron exactamente dos diagnósticos `InvalidMemberAccess`, texto exacto y orden fuente.
- Búsqueda estática: `scanInvalidThisContexts` conserva un solo llamador externo y cero
  auto-llamadas.
- Build focal de `nsc.exe`: terminado; contrato de host runtime estático aprobado.
- Baseline, suite completa, deploy, instalación y red: no ejecutados.

## Estado final

B10B terminado. B10 cerrado.
