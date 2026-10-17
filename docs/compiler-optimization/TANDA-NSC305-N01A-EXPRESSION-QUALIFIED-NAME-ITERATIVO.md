# NSC 3.0.5 — N01A: nombre calificado de expresión iterativo

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`, `20c7c9773`.
- **Entrada roja:** el proyecto focal de 16,384 segmentos usado por G10, ejecutado mediante
  `--check-project-members -O0 -j 1 --progress`.
- **Defecto observado:** después de G10, `generics` termina y el proceso cae con `0xC00000FD`
  dentro de la fase pública `names`.
- **Hipótesis única:** el primer `MemberExpression` profundo llega a
  `ProjectNameLookup::bindExpressionTypeReceiver`, cuyo helper anónimo
  `expressionQualifiedName` desciende recursivamente por toda la espina izquierda y reconstruye
  prefijos al regresar.
- **Instrumentación temporal autorizada:** checkpoints antes/después de
  `expressionQualifiedName` en `A:\temp\nsc-names-trace.log`; deben retirarse antes del commit.
- **Archivo y símbolo autorizados:** `src/Frontend/ProjectNameLookup.cpp`, exclusivamente
  `expressionQualifiedName`; este expediente y tracking. La prueba profunda se conserva en el
  target focal existente.
- **Contratos congelados:** nombres escritos, resolución, candidatos, diagnósticos, orden, API,
  ABI, NSMETA, sintaxis, semántica y artefactos.
- **Protegidos:** `bindScope`, `bindNode`, tipos, members, HIR, MIR, LLVM y consumidores.
- **Prohibido:** mezclar el walker recursivo `bindNode` si aparece como bloqueo posterior; deberá
  transferirse a N01B con evidencia propia. Sin baseline, suite completa, deploy, instalación ni
  red.
- **Gate:** checkpoint rojo dentro del helper; implementación iterativa O(N); checkpoint verde de
  retorno sobre la misma entrada; instrumentación retirada; build focal, diff auditado y commit
  local aislado.

## Cadena causal y contrafactual

```text
names
→ ProjectNameLookup::bindNode(MemberExpression)
→ ProjectNameLookup::bindExpressionTypeReceiver
→ expressionQualifiedName(expression)
→ recursión por 16,384 MemberExpression + prefijos crecientes
→ 0xC00000FD
```

Con la versión roja, `A:\temp\nsc-names-trace.log` contenía
`expressionQualifiedName.before` y no contenía el cierre `after`. Después de sustituir únicamente
el helper, la misma entrada produjo pares `before/after` repetidos. El proceso avanzó a un bloqueo
posterior dentro de `bindNode`, transferido a N01B. La instrumentación se retiró antes del build
limpio.

## Implementación

El helper conserva vistas de cada miembro mientras recorre la espina izquierda en un bucle,
rechaza las mismas formas inválidas, calcula el tamaño final y construye el resultado con una sola
reserva en orden raíz-a-hoja. La complejidad pasa de profundidad nativa O(N) y copias O(N²) a pila
explícita O(N) y ensamblado O(N).

## Evidencia focal

- Rojo: `generics` termina y `names` cae; último checkpoint:
  `expressionQualifiedName.before`.
- Contrafactual: la misma entrada emite `expressionQualifiedName.after` para la cadena profunda.
- Build focal de `nsc.exe`: terminado; contrato de host runtime estático aprobado.
- Búsqueda estática: `expressionQualifiedName` ya no se llama a sí misma.
- El cierre completo de `compileProjectNames` permanece bloqueado por el walker diferente
  `ProjectNameLookup::bindNode`; N01B activo.
- Instrumentación temporal: retirada por completo.

## Estado

N01A terminado. N01B activo.
