# Congelamiento 3.0, elemento 135a — temporales owned prestados como argumento

Fecha: 2026-10-27. Revisión de partida: `16c0e6b0b`.

## Nota de trabajo

```text
HEAD: 16c0e6b0b
Elemento: 135a del plan de 153 (corrección de la cobertura declarada por 135; hallazgo L1 del doc 56)
Contrato vigente: elemento 135 (doc 54) y contrato de vida (known-life-sources.md, elemento 137)
Archivos exactos que se modificarán:
  include/nativesharp/Frontend/ProjectMethodBodyModel.h
  include/nativesharp/Frontend/ProjectOopHirModel.h
  src/Frontend/Bodies/CallAnalyzer.cpp
  src/Frontend/HIR/HirExceptionAndCleanup.cpp
  tests/unit/Freeze30ContractTests.cpp
  docs/3.0/language/known-life-sources.md
  docs/3.0/NSC-ESTADO-CONGELAMIENTO-3.0.md
  docs/3.0/decisiones-congelamiento/58-TEMPORALES-PRESTADOS-COMO-ARGUMENTO.md
```

## Problema

Dos defectos, en el árbol de partida (sondas `A:\temp\nsc-dec\d43`):

- `string s = Id(MakeText());` compilaba. El temporal owned que la llamada toma prestado se destruía al final de
  la expresión y `s` lo seguía apuntando.
- `Id(MakeText()).Length` leía el temporal ya destruido. Su `FullExpressionCleanup` envolvía sólo la llamada y
  no la expresión completa (volcado HIR de C04).

## Decisión de Alberto (2026-10-27)

- El resultado no owned de una llamada hereda conservadoramente la procedencia de todo argumento o receptor de
  vida conocida. Quedan fuera un retorno owned y un valor simple.
- No es un parche para string: vale para cualquier referencia no owned que la llamada pueda devolver.
- Si el resultado escapa y el dueño no puede prolongarse, se rechaza.
- Los derivados de `raii`, `scope` y `catch` no se amplían (decisión de 136).

## Contrato

1. **Análisis.**
   - `markCallTemporaryBorrow` marca como préstamo el resultado referencia o string no owned de una llamada
     cuando:
     - el receptor es un préstamo o un temporal;
     - algún argumento es un préstamo;
     - algún argumento es el propio temporal owned (llamada con retorno owned o `new` de clase, de cualquier
       tipo referencia salvo delegate) entregado a un parámetro que no toma ownership.
   - Vale para llamadas a métodos, funciones de namespace (incluido el receptor reenviado de una extensión) y
     delegates. En un delegate declarado se leen los parámetros `owned` y el retorno `owned`.
   - Los sumideros de 131a rechazan el resultado: local, asignación, `return` y `yield`.
2. **HIR.**
   - `extendLentArgumentCleanups` quita el `FullExpressionCleanup` propio de una llamada que presta un temporal
     argumento y devuelve una referencia no owned.
   - Cierra en su lugar la expresión completa con la misma regla que un temporal receptor
     (`planFullExpressionClose`).
   - El temporal vive mientras la expresión lo lee y se destruye una vez.

Un temporal de clase nunca llega a un parámetro que no toma ownership: C-01 lo rechaza antes (sondas C10–C13).
La regla de 135a lo cubre igual, sin distinguir tipos.

## SCOPE LOCK

- **Cambia.**
  - Guardar, asignar, devolver o entregar con `yield` el resultado no owned de una llamada que presta un temporal
    owned argumento es error.
  - La lectura de ese resultado dentro de la expresión ve el temporal vivo.
  - El contrato de vida deja de listar esa forma como límite.
- **Idéntico.**
  - Retornos owned.
  - Valores simples.
  - Los derivados de `raii`, `scope` y `catch` (límites visibles del contrato).
  - Los demás diagnósticos (golden d41 idéntico).

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol. Sondas d43 en `-O0` y `-O2`, con el mismo
resultado en los dos:

| Sonda | Forma | Antes | Ahora |
|---|---|---|---|
| C01 | `string s = Id(MakeText());` | 3 | rechazada |
| C02 | `Id(MakeText()).Length` | 3 (temporal ya destruido) | 3, temporal vivo |
| C03 | `string s = Id(Id(MakeText()));` | 3 | rechazada |
| C04 | `Id(Id(MakeText())).Length` | 3 | 3; la limpieza envuelve toda la expresión |
| C05 | `raii string c = Copy(MakeText());` (retorno owned) | 4 | 4 |
| C06 | `Len(MakeText())` (valor simple) | 3 | 3 |
| C07, C08 | `return Id(MakeText());`, `s = Id(MakeText());` | 3 | rechazadas |
| C14, C15 | delegate `f(MakeText())` guardado / leído | 3, 3 | rechazada, 3 |
| C17 | `string s = Same(MakeText());` (genérico) | 3 | rechazada |
| C18, C19 | `borrowed string`: guardado / leído | rechazada, 3 | rechazada, 3 |
| C21, C22 | `Make(4).Self()` guardado / leído (receptor) | — | rechazada, 44 |
| C23 | `Id(MakeText()).Length + Id(MakeText()).Length` | — | 6 |
| C24 | `if (Id(MakeText()).Length == 3)` | — | 7 |
| C10–C13, C16 | temporal de clase a parámetro no owned | rechazadas (C-01) | sin cambio |

Sondas d41 (golden de 136): idénticas. d40 (135): sin cambio.

Resto: `seq.cmd f135a` contra `f136` es idéntico en todo. Incluye:

- `all.cmd`;
- `asynccases`, `gencases` y `loopcases`;
- los 66 programas de Freeze30 en `-O0` y `-O2`;
- los 27 proyectos de `tests/cases`;
- d28;
- las bibliotecas.

Prueba nueva: `Freeze30ContractTests::testLentArgumentTemporaries`.

- Corren:
  - F135aStringRead: 6;
  - F135aNestedRead: 3;
  - F135aOwnedReturn: 4;
  - F135aPlainValue: 3;
  - F135aClassReceiverRead: 44.
- Rechazadas: F135aStringStored, F135aNestedStored, F135aDelegateStored, F135aClassReceiverStored y
  F135aClassArgument (C-01).

Compila en `A:\temp\nsc-dec\bt`; se ejecuta en el gate final.

## No verificado

- Batería completa y unitarias: gate final.
- Un delegate de tipo de función estructural no declara parámetros `owned`; todos sus argumentos prestan.
