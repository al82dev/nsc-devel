# Arquitectura de la nueva suite del compilador

## 1. Decisión

La suite nueva se implementará desde cero y coexistirá con la batería histórica sin modificarla. La unidad de
diseño es el **contrato observable**, no el archivo de prueba legado, el número de un `if`, una versión interna del
lenguaje ni una etapa histórica.

La arquitectura admite solamente dos superficies nuevas de ejecución:

1. `nsc_component_tests`: un único ejecutable C++ con módulos por componente host;
2. `nsc_suite_controller + nsc`: un controlador sin semántica propia que conduce el `nsc.exe` construido, NSLink y
   las herramientas de proceso requeridas por fixtures declarativos.

La batería histórica es una tercera autoridad sólo por conservación: se ejecuta intacta como referencia semanal y
no se transforma en runner nuevo.

## 2. Universo cerrado por la Fase 1

| Elemento | Total | Fuente canónica |
|---|---:|---|
| Archivos auditados | 974 | `inventory/production-files.csv` |
| Componentes con identidad única | 57 | `inventory/components.csv` |
| Símbolos/artefactos inventariados | 8,401 | `inventory/symbols.csv` |
| Contratos | 1,309 | `inventory/contracts.csv` |
| Casos diseñados | 10,193 | `inventory/test-case-design.csv` |
| Casos de la suite nueva | 9,773 | `inventory/suite-case-allocation.csv` |
| Casos de referencia histórica congelada | 420 | `inventory/suite-case-allocation.csv` |

El total no es una cuota. Es la suma derivada de los contratos auditados y puede cambiar únicamente mediante una
actualización explícita del inventario, el plan del componente y la matriz de casos.

## 3. Fronteras de responsabilidad

### 3.1 `nsc_component_tests`

Es un solo binario, no un ejecutable por archivo ni por caso. Contiene un registro común y 24 módulos host:

`AST`, `Basic`, `CFG`, `Compilation`, `Diagnostics`, `Dominance`, `Driver`, `Frontend`, `HIR`, `Interop`,
`Language`, `LanguageService`, `Lexer`, `Linker`, `LLVM`, `MIR`, `nsc`, `Parser`, `Passes`, `Runtime`, `Sema`,
`Source`, `SSA` y `Target`.

Cada módulo puede usar varias unidades `.cpp` cuando el costo de compilación o el aislamiento lo justifique, pero
la división física nunca será una unidad por prueba. Los casos se registran por ID estable y se seleccionan por
caso, contrato, componente, capa o perfil.

Esta superficie posee 6,118 casos:

- `unit`: lógica aislable y valores puros;
- `component`: colaboración interna estable de un componente host.

Los tests se escriben contra interfaces o adaptadores estrechos. No se acoplan a la clase monolítica que hoy
implementa un contrato si el contrato puede sobrevivir a su futura modularización. Una refactorización de
`ProjectMethodBodyModel`, por ejemplo, puede cambiar el adaptador, pero no obliga a reescribir los casos semánticos.

### 3.2 `nsc_suite_controller + nsc`

El controlador no reimplementa el lexer, parser, semántica, linker ni diagnósticos. Sus únicas funciones son:

- leer el manifiesto validado;
- seleccionar casos y configuraciones;
- crear un directorio temporal limpio por caso;
- invocar rutas explícitas de `nsc.exe` y `nslink.exe`;
- capturar salida, código, artefactos y tiempos;
- comparar con el oráculo declarado;
- emitir eventos y reportes progresivos.

El compilador construido es quien procesa los programas `.ns`. Para fixtures ejecutables, NSLink es el linker de
producto. `lld-link` puede seguir formando parte de la construcción C++ del host, pero no sustituye a NSLink en una
prueba de producto NativeSharp.

Esta superficie posee 3,655 casos de integración, proceso, robustez, bibliotecas, self-host, build y empaquetado.
No todos generan un `.exe`: un caso de tokens, AST, semántica, IR u objeto termina en el artefacto mínimo necesario.
Sólo se enlaza y ejecuta cuando el contrato requiere conducta de runtime.

### 3.3 Batería histórica congelada

Los 420 casos clasificados `historical-weekly` conservan su infraestructura, nombres y expectativas actuales. No
son la fuente de diseño de la suite nueva ni bloquean su organización. Su propósito es detectar una divergencia de
largo alcance una vez por semana o bajo solicitud explícita.

No se copiará su lógica a `nsc_component_tests` ni al controlador. Si un contrato vigente ya está diseñado en la
suite nueva, la prueba histórica queda como evidencia redundante deliberada, no como autoridad primaria.

## 4. Árbol futuro, todavía no implementado

```text
tests/compiler-suite/
  manifest/
    suite.schema.json
    cases.jsonl
    components.json
  host/
    main.cpp
    framework/
    modules/
      basic/
      lexer/
      parser/
      frontend/
      ...
  cases/
    language/
    integration/
    runtime/
    libraries/
    selfhost/
    build/
  controller/
    nsc_suite.py
  expected/
  reports/
```

Este árbol no autoriza su creación en la Fase 1. Define la frontera que implementará la Fase 2.

## 5. Manifiesto y fuente de verdad

Cada caso materializado tendrá, como mínimo:

| Campo | Regla |
|---|---|
| `case_id` | ID estable y único del inventario |
| `contract_id` | contrato existente y único |
| `component` | uno de los 57 componentes canónicos |
| `class` | nominal, variante válida, inválido, límite, recuperación o determinismo |
| `executor` | una de las tres autoridades descritas arriba |
| `inputs` | archivos, argumentos y estado explícitos |
| `oracle` | resultado estructural o observable exacto |
| `dependencies` | productores reales requeridos |
| `profile` | PR, NIGHTLY o WEEKLY |
| `build_modes` | Debug, Release o ambos |
| `optimizations` | O0, O1, O2, O3 o no aplicable |
| `isolation` | reset en proceso o directorio temporal nuevo |
| `timeout_class` | presupuesto por clase, no tiempo mágico por caso |

El descubrimiento será dinámico desde el manifiesto. Nunca se codificará “se esperan N pruebas” como criterio de
corrección. El sistema valida unicidad, referencias y estructura del manifiesto y después ejecuta todo lo que
descubrió para el filtro solicitado.

## 6. Contratos de oráculo

Un caso falla sólo por una divergencia semántica declarada. Se prohíbe usar como oráculo accidental:

- versión exacta de Python, CMake o Ninja;
- hash de una herramienta auxiliar que no forma parte del producto probado;
- puntuación, espacios o texto incidental no contractual;
- paths absolutos de una máquina;
- cantidad rígida de pruebas descubiertas;
- timestamp, duración o orden de workers cuando no son semánticos;
- artefactos viejos, cachés o fallback después de fallar un productor.

Cuando el texto sea el contrato —por ejemplo un diagnóstico público— se compara de manera exacta. En los demás
casos se usa una representación estructurada: código, ID de diagnóstico, token, árbol, modelo semántico, IR
normalizado, metadata, tabla PE, salida del proceso o estado de recursos.

## 7. Aislamiento y causalidad

- Cada caso conducido por `nsc.exe` usa un directorio temporal nuevo.
- Los módulos host restablecen todo estado mutable entre casos.
- Un productor fallido marca a sus consumidores como `BLOQUEADO_POR`; no los reporta como defectos independientes.
- Ningún consumidor utiliza un artefacto anterior para continuar.
- La paralelización sólo ocurre entre casos cuyo manifiesto declara independencia.
- Los resultados se ordenan por ID estable, no por orden de finalización.
- Una cancelación deja un reporte legible con el último evento persistido.

## 8. Configuración ortogonal

Construir, configurar la suite y ejecutarla son operaciones distintas:

1. **Build:** producir `nsc.exe` en Debug o Release y las herramientas explícitas requeridas.
2. **Configure:** validar manifiestos, rutas y capacidades; no compilar el producto otra vez.
3. **Execute:** seleccionar perfiles, componentes, casos, build mode y optimizaciones.
4. **Report:** condensar resultados sin mezclar la salida de construcción con la salida de pruebas.

La configuración nunca busca silenciosamente otra copia de NSC o NSLink. Las rutas se proporcionan de forma
explícita y se registran en el reporte. La Fase 2 trabajará en `A:`; no habrá dependencia implícita de un despliegue
en `C:`.

## 9. Optimización y build mode

- Los casos host no tienen nivel `O0..O3`; se ejecutan contra el binario Debug o Release seleccionado.
- Los casos procesados por NSC admiten `O0`, `O1`, `O2` y `O3` cuando el contrato lo permite.
- El nivel predeterminado para desarrollo es `O0`.
- Las matrices múltiples pertenecen a NIGHTLY o a una solicitud explícita; PR no repite niveles sin una razón de
  contrato.
- Elegir `todos` significa cuatro ejecuciones identificables del mismo caso, no cuatro casos distintos.

## 10. Reportes

Cada corrida produce fuera del código fuente:

- `events.jsonl`, escrito progresivamente;
- `results.json`, resultado estructurado completo;
- `summary.md`, resumen humano;
- `failures.log`, evidencia completa sólo de fallos y bloqueos;
- un subdirectorio por caso con comandos, stdout, stderr y artefactos declarados.

Los estados son `PASS`, `FAIL`, `BLOCKED`, `SKIPPED`, `CANCELLED` e `INFRASTRUCTURE_ERROR`. Un error de build o
configuración no se mezcla con un fallo del lenguaje. El reporte identifica revisión, hashes de NSC/NSLink, modo,
optimización, perfil y filtros usados.

## 11. Evolución

Agregar o cambiar un feature exige, en el mismo cambio:

1. actualizar el contrato del componente;
2. añadir o modificar casos diseñados;
3. actualizar la asignación de suite;
4. implementar esos casos en la Fase 2;
5. ejecutar el perfil requerido;
6. conservar evidencia del resultado.

No se mantiene manualmente un conteo de ramas o `if`. La cobertura instrumental y mutation testing pertenecen a
una fase posterior y calculan sobre el código vigente. Los IDs contractuales permanecen estables mientras la
conducta pública siga existiendo, aunque cambie la estructura interna.

## 12. Gates de implementación

La arquitectura no puede entrar en Fase 2 hasta que:

- los 10,193 casos tengan una asignación exactamente una vez;
- los 57 componentes tengan una ola de implementación o una exclusión explícita;
- todo contrato tenga al menos un caso y todo caso un contrato existente;
- no haya componente, ejecutor, perfil ni optimización desconocidos;
- la segunda auditoría independiente reproduzca los conteos desde cero;
- la batería histórica y su infraestructura conserven sus hashes congelados.
