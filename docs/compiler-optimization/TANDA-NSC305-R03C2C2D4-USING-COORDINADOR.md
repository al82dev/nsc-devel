# NSC 3.0.5 — R03C2C2D4: `using` en el coordinador compartido

## SCOPE LOCK

Esta hoja migra exclusivamente `using` desde la ruta `Direct` al
`Parser::ParseContinuationCoordinator`. Quedan fuera `defer` (D5), switch/match/patrones
condicionales (E1–E3), el cierre F, Pratt, HIR, MIR, LLVM, sintaxis, ABI y NSMETA.

No se modifica el lowering de cleanup ni se elimina el parser histórico todavía requerido por
familias no migradas. El checkout principal y ambos trackings permanecen fuera de este commit.

## Contrato preservado

- Gramática: `using (` tipo nombre `=` initializer `)` bloque obligatorio.
- Diagnósticos y orden histórico de `expect` sin cambios.
- `UsingStatement.typeSyntax`, `text`, `first` y `second` conservan respectivamente tipo, nombre,
  initializer y body.
- El initializer se analiza exactamente una vez antes del body.
- El body exige llaves; recovery, cursor y rango completo permanecen idénticos.
- El anidamiento conserva el orden estructural que consume el lowering LIFO existente.

## Implementación

`StatementKind::Using` clasifica `KwUsing` antes del fallback `Direct`. `UsingFrame` conserva
inicio, slot de resultado, tipo, nombre y nodo. `startUsing`, `finishUsingInitializer`,
`scheduleUsingBody`, `finishUsingBody` y `completeUsing` dividen header, initializer y body en
continuaciones explícitas. Expression y Block se solicitan mediante la worklist compartida, sin
abrir una fachada o `drain` anidado para esta familia.

Todas las funciones nuevas permanecen por debajo de 60 líneas ejecutables.

## Prueba puntual e infraestructura

`TP-PAR-559` (`Host.Parser`, `CP-PAR-035`, `NIGHTLY`) contiene cuatro probes aislados dentro de
un único selector:

1. 32,768 `using` anidados con validación iterativa del AST;
2. tipo, nombre, initializer, body, orden de recursos y cursor posterior;
3. initializer lambda-block para cubrir la frontera expression→block del coordinador;
4. recovery de `=` o body ausente preservando el statement siguiente.

La cadena está registrada una sola vez en módulo, CMake de compiler-suite, `TestRegistry.h`,
registro default, design CSV, allocation CSV y manifest JSONL.

## Evidencia

- Primer build focal: detuvo correctamente en `-Wmissing-field-initializers` para `UsingFrame`;
  se sustituyó la inicialización agregada parcial por inicialización explícita, sin cambiar lógica.
- Rebuild exclusivo `nsc_component_tests`: PASS, 113/113 pasos, exit 0.
- Única prueba ejecutada, `nsc_component_tests.exe --case TP-PAR-559`: PASS, exit 0,
  330,656 microsegundos; pasaron los cuatro probes, incluido el contrafactual 32,768.
- Source-registration, CanonicalCatalog 6,014 con TP-PAR-559, símbolos, límite de funciones y
  `git diff --check`: PASS.

Logs persistentes:

- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2d4-build-nsc-component-tests.log`
- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2d4-build-nsc-component-tests-retry.log`
- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2d4-tp-par-559.log`

No se ejecutaron Host.Parser completo, CTest, TL-LANG, regresiones, baseline, deploy ni
instalación.
