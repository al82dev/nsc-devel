# NSC 3.0.5 — B10A: `scanMethodBody` iterativo

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`, `736cce40f`.
- **Entrada roja:** método ordinario cuyo return contiene una llamada con receptor calificado de
  16,384 `MemberExpression`; parser, `generics` y `names` ya cuentan con gates propios para esta
  profundidad.
- **Operación candidata:** `ProjectMemberModel::scanMethodBody`, que se auto-invoca por
  `first/second/third/children` y por dos ramas especiales.
- **Naturaleza:** refactor interno no funcional para retirar profundidad de pila nativa.
- **Archivo/símbolo autorizado:** `src/Frontend/ProjectMemberModel.cpp`, sólo
  `scanMethodBody`; `tests/unit/ProjectMemberModelTests.cpp`, expediente y tracking.
- **Orden congelado:** preorden normal `first`, `second`, `third`, `children`; para `this.member`,
  binding de `this` antes de resolver/publicar el acceso y retorno temprano; para `this->member`,
  diagnóstico antes del binding de `this` y retorno temprano.
- **Contratos congelados:** contenido y orden de `bodyBindings_`, candidatos, diagnósticos y orden,
  sintaxis, semántica, API, ABI, NSMETA y artefactos.
- **Protegidos:** `scanInvalidThisContexts`, parser, generics, names, method-bodies, HIR, MIR, LLVM
  y consumidores.
- **Prohibido:** mezclar B10B, modificar límites, reducir profundidad, baseline, suite completa,
  deploy, instalación o red.
- **Gate:** rojo propio con `0xC00000FD`; algoritmo iterativo con continuaciones explícitas para
  las ramas especiales; target focal completo verde; conteo/orden existente de bindings intacto;
  build focal de `nsc`; diff y commit local aislado.

## Cadena causal

```text
método Run con receptor de 16,384 MemberExpression
→ ProjectMemberModel::build
→ scanMethodBody(method.syntax->first())
→ auto-llamada por first() una vez por segmento
→ 0xC00000FD
```

El target focal sin la corrección terminó con `native-exit=-1073741571`. Parser, generics y names
ya habían sido corregidos y la entrada está dentro de una declaración de método, por lo que no
participa `scanInvalidThisContexts` en el descenso profundo.

## Implementación

`scanMethodBody` usa una pila LIFO de `WorkItem`. `Visit` conserva el preorden ordinario mediante
push inverso. `CompleteThisMember` representa la continuación que debe ejecutarse después de
visitar `this`: resuelve candidatos, conserva el diagnóstico `UnknownMember` y publica
`ThisMemberAccess` en la misma posición que la versión recursiva. La ruta sintética `this->member`
publica primero su diagnóstico y agenda después el binding de `this`, conservando el orden previo.

## Evidencia focal

- Rojo propio: `native-exit=-1073741571` (`0xC00000FD`).
- Target `nsc_project_member_model_tests.exe`: `PASS: 71 checks`.
- La prueba nueva completa una expresión de 16,384 segmentos dentro de método.
- Los checks existentes mantienen tres `ThisReference`, tres `ThisMemberAccess` y candidatos
  únicos para `this.Value`; diagnósticos de cuerpos continúan verdes en el mismo target.
- Build focal de `nsc.exe`: terminado; contrato de host runtime estático aprobado.
- Búsqueda estática: no quedan auto-llamadas de `scanMethodBody`; el único sitio externo permanece
  en `ProjectMemberModel::build`.
- Baseline, suite completa, deploy e instalación: no ejecutados.

## Estado

B10A terminado. B10B pendiente.
