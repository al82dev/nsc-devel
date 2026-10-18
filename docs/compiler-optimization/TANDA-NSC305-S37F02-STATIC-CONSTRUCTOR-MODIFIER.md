# Tanda NSC 3.0.5 S37F02 — identidad de constructor estático

Fecha: 2026-10-17
Rama: `development`
Revisión de partida: `cf9ca363bebf0c0d19b26798f20f1c77fd3d2f3d`

## SCOPE LOCK

| Dimensión | Contrato de la tanda |
|---|---|
| Objetivo | Restaurar la distinción canónica entre constructor estático y constructor de instancia. |
| Naturaleza | Corrección focal de una regresión de movimiento físico en `MemberParser`. |
| Causa | `parseConstructorTail` guardaba `isStatic` en `AstNode::flag`; `ProjectMemberModel` consulta `hasModifier(KwStatic)`. |
| Corrección | Restaurar `KwStatic` sólo cuando `isStatic` es verdadero, igual que antes de R01C. |
| Archivos autorizados | `src/Parser/MemberParser.cpp`, este expediente y el tracking externo. |
| Archivos protegidos | `StatementParser`/R01E, HIR, MIR, LLVM, ABI, NSMETA, fixtures y contratos del lenguaje. |
| Prohibido | Cambiar la fixture, relajar el diagnóstico de duplicados, ejecutar baseline o una suite completa. |
| Terminación | Frontend y component host construyen; TL-LANG-295 y Stage37 pasan focalmente. |

## Diagnóstico

La fixture válida declara ambos constructores:

```text
static Gate() { Value = 7; }
public Gate() { }
```

El parser sí reconocía el primer encabezado como estático, pero `parseConstructorTail` ejecutaba
`constructor->setFlag(isStatic)`. El modelo de miembros no consulta ese flag: deriva
`ClassMemberSymbol::isStatic` de `memberNode->hasModifier(KwStatic)`. Ambos constructores llegaban,
por tanto, como instancias y colisionaban con `duplicate constructor signature`.

La comparación histórica con el cuerpo inmediatamente anterior a la extracción R01C confirmó el
contrato original: `if (isStatic) constructor->addModifier(TokenKind::KwStatic)`. La corrección
restaura exactamente esa operación; no cambia la gramática, el AST de constructores de instancia,
los parámetros, inicializadores, cuerpos ni diagnósticos.

## Validación focal

Todos los temporales y logs permanecen en `A:\temp`.

| Gate | Resultado | Evidencia |
|---|---|---|
| Build `nsc_frontend` | PASS, exit 0 | `A:\temp\s37f02-build-nsc-frontend.log` |
| Build `nsc_component_tests` | PASS, exit 0 | `A:\temp\s37f02-build-component-host.log` |
| `TL-LANG-295` | PASS, exit 0, 90,577 us | `A:\temp\s37f02-tl-lang-295.log` |
| Build `nsc_stage37_static_initialization_tests` | PASS, exit 0 | `A:\temp\s37f02-build-stage37.log` |
| Stage37 directo | PASS, 72 checks, exit 0 | `A:\temp\s37f02-stage37-direct.log` |
| CTest `nsc.stage37.static-initialization` | PASS 1/1, exit 0 | `A:\temp\s37f02-ctest-stage37.log` |

`TL-LANG-295` verifica además que el constructor estático no recibe `this` ni parámetros implícitos,
queda fuera del overload set de construcción de instancia y conserva su cuerpo MIR estático. Sus
contrafactuales de accesibilidad, parámetros, inicializador, duplicado estático y uso de `this`
también conservaron los diagnósticos estrictos.

## Resultado

S37F02 queda cerrado con una sola restauración de contrato en producción. La corrección no forma
parte del movimiento R01E y se integra en un commit separado para mantener una hipótesis por commit.
