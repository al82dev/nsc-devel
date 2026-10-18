# H02S13N — assignments direct/compound HIR iterativos

## SCOPE LOCK

- Repositorio: clon local `A:\NSC-devel-paralelo\development-agente-1-hir`.
- Rama: `codex/development-agente-1-hir`.
- Base de la hoja: H02S13M en `b875bcf8dd22102bc76621c3fca48581bc05fdae`.
- Cambio: migrar únicamente assignment directo y compound directo a la máquina HIR iterativa.
- Preservado: sintaxis, API, ABI, NSMETA, diagnósticos, orden address→RHS, semántica string,
  conversiones, ownership, `localOrdinal`, `requiresValueDrop`, forma y tipos HIR.
- Protegido: H02S13Q y posteriores, Parser, MIR, LLVM, tracking y repositorio integrador.
- Validación autorizada: compilar sólo `nsc_project_oop_lowering_tests` y ejecutar sólo el selector
  `hir-direct-compound-assignment-depth`.
- Prohibido: otras pruebas, CTest, suite, baseline, deploy, instalación y red.

## Causa

Las ramas históricas de assignment directo y compound directo permanecían dentro de
`lowerExpressionImpl`. El store simple reentraba para bajar RHS; el compound reentraba primero
para obtener el valor actual y después para bajar RHS. En el caso string compound existía además
la concatenación intermedia. Una cadena profunda por cualquiera de esas aristas mantenía
profundidad proporcional en el stack nativo y dejaba incompleta la frontera iterativa HIR.

## Diseño implementado

`HirExpressionMachine` reconoce tres estados explícitos: `DirectAssignment`,
`CompoundAssignment` y `StringCompoundAssignment`. La clasificación sólo toma estados válidos
que la ruta histórica habría procesado; los estados inválidos conservan la ruta y diagnósticos
anteriores.

Cada frame baja sus dependencias mediante la máquina común y publica al desenrollarse en LIFO.
El assignment directo conserva address→RHS y emite el store con el type key y ownership originales.
El compound numérico conserva address→valor actual→RHS, `compoundAssignmentBaseOperator`, las
conversiones históricas y el store final. El compound string conserva address→valor actual→RHS,
la concatenación exacta y sus temporales/ownership. En los tres casos permanecen sin cambio
`ownsStorageDestination`, `requiresValueDrop`, `localOrdinal`, el rechazo de address context y los
mensajes exactos para target type u operador inválidos.

No se modifica `lowerExpressionImpl` fuera de desviar los estados válidos hacia la máquina; esa
ruta histórica permanece como referencia/fallback para estados que no cumplen la clasificación.

## Prueba puntual e infraestructura

El target existente `nsc_project_oop_lowering_tests` incorpora un único selector:

```text
NSC_PROJECT_OOP_LOWERING_FOCUS=hir-direct-compound-assignment-depth
```

El foco verifica los contratos puntuales de string compound, ownership, drop y ordinal, address
context y diagnósticos inválidos. Después construye una cadena directa de 32,768 assignments y
otra compound de 32,768, rotando los diez operadores compound, y comprueba shape, orden,
operación, type key y ausencia de recursión nativa proporcional.

No se crea target, CTest, manifiesto, catálogo ni entrada de Builder nuevos: el selector y la ruta
default pertenecen al ejecutable unitario existente.

## Incidente de configuración aislada

El primer intento aislado reconfiguró accidentalmente el `build-clang` del clon como Debug y luego
Release. Ambas compilaciones aceptaron `HirExpressionMachine.cpp` y
`ProjectOopLoweringTests.cpp`, pero el enlace detectó mezclas preexistentes/incompatibles de
`_ITERATOR_DEBUG_LEVEL` y después de `MT_StaticRelease`/`MD_DynamicRelease` entre bibliotecas.
Esos fallos no son evidencia funcional de la hoja. Ese árbol quedó descartado y no se reparó ni
se reutilizó. La validación oficial se ejecutó en
`A:\NSC-devel-paralelo\builds\agente-1-s13n`, directorio nuevo que reproduce la configuración
exitosa comprobada de S13M: Ninja, Debug, `clang-cl` 23.1 MSVC-like, CRT estático,
`_ITERATOR_DEBUG_LEVEL=0`, host `STATIC/O2` y LLVM 23.1 de una sola raíz.

## Evidencia

- Configure aislado: PASS.
- Build exacto: `nsc_project_oop_lowering_tests`, PASS, exit 0.
- Selector puntual: `hir-direct-compound-assignment-depth`, PASS 10/10, exit 0.
- Profundidad validada: 32,768 assignments directos y 32,768 compound.
- Logs: `A:\NSC-devel-paralelo\logs\agente-1\H02S13N-configure-isolated.log`,
  `A:\NSC-devel-paralelo\logs\agente-1\H02S13N-build-isolated.log` y
  `A:\NSC-devel-paralelo\logs\agente-1\H02S13N-focus.log`.
- Otras pruebas ejecutadas: ninguna.
- Auditoría estática: `git diff --check` limpio; las rutas válidas publican sólo al desenrollarse y
  la clasificación deja los estados inválidos en la ruta histórica.
