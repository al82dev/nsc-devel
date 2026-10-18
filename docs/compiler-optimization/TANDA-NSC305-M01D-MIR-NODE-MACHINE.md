# Tanda NSC 3.0.5 M01D — extracción de `MirNodeMachine`

## Clasificación documental

- **IMPLEMENTACIÓN VIGENTE:** refactor interno descrito en este expediente.
- **HECHO COMPROBADO:** observación directa del árbol, diferencial o auditoría estática.
- **NO VERIFICADO:** comportamiento que requiere compilación o ejecución y queda diferido.

## Propósito

M01D extrae de `MirBuildCoordinator.cpp` la máquina que transforma un nodo HIR en su nodo MIR.
Conserva la recursión, el orden de las 273 clases HIR, las copias de campos, los diagnósticos, las salidas
tempranas y el recorrido posterior de hijos. No implementa la migración iterativa de M02C.

## SCOPE LOCK

| Dimensión | Alcance vinculante |
|---|---|
| Repositorio y rama | `A:\nsc-devel`, rama `development`. |
| Revisión base | `e5de9a9508bdad45477bf1248ee7114b1021d9a1`. |
| Naturaleza | Extracción física y descomposición mecánica, sin cambio funcional. |
| Oráculo | `ProjectOopMirModel::lowerNode` de la revisión base. |
| Contratos preservados | Sintaxis, semántica, API, ABI, NSMETA, diagnósticos, orden y formatos. |
| Algoritmos preservados | Mapeo HIR→MIR, fallos, lookup, offsets, llamadas y recorrido recursivo. |
| Archivos de producto | Header privado MIR, coordinador y nuevo `MirNodeMachine.cpp`. |
| Infraestructura | CMake raíz y CMake de compiler-suite. |
| Documento | Este expediente. |
| Trabajo protegido | Parser, HIR, pruebas y documentos concurrentes ajenos. |
| Validación autorizada | Auditoría estática, inventarios, diff y registros de infraestructura. |
| Validación prohibida | Build, pruebas, baseline, deploy, instalación y scripts de producto/prueba. |
| Criterio de cierre | Extracción completa, cero funciones mayores de 60 y alta de fuente completa. |

## Frontera extraída

La revisión base contenía `lowerNode` en las líneas 2488–4128 de
`MirBuildCoordinator.cpp`: 1,641 líneas. El bloque se trasladó primero de forma textual a la nueva unidad.
Su SHA-256 normalizado en LF es:

`4E1B45BBDF2AB5A47FF1283D519DD043698BD894890DEC1DE3207D6B80A03D8A`

El estado final deja:

- `MirBuildCoordinator.cpp`: 2,542 líneas;
- `MirNodeMachine.cpp`: 2,254 líneas;
- `ProjectOopMirModel::lowerNode`: wrapper privado de una línea ejecutable;
- `ProjectOopMirModel::MirNodeMachine`: un único tipo privado anidado, sin estado persistente nuevo.

El tipo anidado no agrega campo a `ProjectOopMirModel`, no cambia su layout y no introduce un nombre
público de API. Conserva una referencia al modelo exclusivamente durante la llamada.

## Descomposición mecánica

El lineamiento de tamaño impide mover el monolito como una sola función. La máquina se divide en:

1. tres helpers de copia de campos HIR→MIR;
2. 24 grupos contiguos del switch original;
3. un dispatcher que consulta esos grupos en el mismo orden;
4. un helper para el recorrido recursivo de hijos;
5. el wrapper privado original de `ProjectOopMirModel`.

Cada grupo conserva segmentos atómicos completos. Las etiquetas con fallthrough permanecen juntas. Un
grupo devuelve `0` si no reconoce la clase, `1` al completar la rama y `-1` donde el código original
devolvía `nullptr`. El dispatcher transforma sólo ese estado interno de nuevo a éxito o fallo.

No se sustituyó recursión por worklist. `lowerChildren` continúa visitando los hijos en su orden original y
llama recursivamente a `MirNodeMachine::lower`. La migración iterativa corresponde exclusivamente a M02C.

## Paridad estática

La comparación contra la revisión base produjo:

- clases HIR del switch: 273/273, secuencia ordenada idéntica;
- clases MIR referenciadas: 276/276 únicas, sin faltantes ni extras;
- campos `result`: 104/104 únicos, sin faltantes ni extras;
- campos `node`: 89/89 únicos, sin faltantes ni extras;
- literales ejecutables: mismos 73; el único literal adicional del archivo es su include;
- salidas fallidas internas: 64 `return nullptr` del switch convertidos en 64 estados `-1`;
- las otras dos salidas `nullptr` originales permanecen en entrada nula y fallo de un hijo;
- copia común: 96 líneas productivas preservadas como subsecuencia ordenada;
- switch: 1,486 líneas productivas preservadas como subsecuencia ordenada;
- recorrido de hijos: 24 líneas productivas preservadas como subsecuencia ordenada.

`stableTypeId` conserva su algoritmo FNV-1a de 64 bits con enlace interno. El coordinador mantiene su copia
porque también la usa al construir tablas de despacho; la unidad nueva contiene la misma función privada
para no crear API ni dependencia pública nueva.

## Límite por función

Se midieron 32 funciones en la nueva unidad:

- funciones por encima de 60 líneas ejecutables: 0;
- máximo: 53 líneas ejecutables;
- `lowerKind`: 49;
- `lowerChildren`: 12;
- `MirNodeMachine::lower`: 8;
- wrapper `ProjectOopMirModel::lowerNode`: 1.

## Infraestructura completa aplicable

### Producto

- `src/Frontend/MIR/MirNodeMachine.cpp` aparece exactamente una vez en `nsc_frontend` del CMake raíz.
- El archivo existe en la ruta registrada.
- No se agrega target: la fuente pertenece al target existente `nsc_frontend`.

### Compiler suite

- La fuente aparece exactamente una vez en `nsc_component_tests`.
- El inventario estático producto/suite contiene 76 fuentes frontend en ambos lados y 0 faltantes.
- `validate_source_registration.py` compara precisamente `nsc_frontend` con `nsc_component_tests`.
- El CMake de compiler-suite invoca el validador durante configure.
- No se ejecutó el validador por la prohibición vigente de ejecutar scripts de prueba.

### Pruebas, catálogo, manifiesto y Builder 3

- M01D no agrega prueba, caso, fixture ni ejecutable; por eso no requiere `add_test` ni registro de caso.
- La cobertura existente permanece en `nsc_project_oop_lowering_tests`, target
  `nsc_project_oop_lowering_tests` y prueba `nsc.project-oop-lowering`.
- No aplica alta en catálogo o manifiesto: no existe identidad de prueba nueva.
- No aplica ruta default nueva: la fuente viaja dentro de los targets existentes.
- Builder 3 configura el CMake raíz para producto y el CMake de compiler-suite para la suite; no mantiene
  un inventario paralelo de fuentes frontend.

## Validación diferida

`VALIDACIÓN DINÁMICA DIFERIDA A LA TANDA INTEGRAL`.

No se ejecutaron compilación, pruebas, configure, baseline ni producto. Por tanto, este expediente afirma
sólo cierre estático. Build, regresión funcional, identidad de artefactos y rendimiento permanecen
**NO VERIFICADOS** hasta la corrida integral indicada por el usuario.

## Estado

- Movimiento y descomposición estática: listos.
- Paridad de inventarios y orden: aprobada.
- Funciones mayores de 60 líneas ejecutables: cero.
- Infraestructura producto/suite: completa por inspección estática.
- Índice Git: debe permanecer vacío hasta la integración aislada.
- Commit: no creado por instrucción vigente.

`M01D LISTO PARA INTEGRACIÓN ESTÁTICA; VALIDACIÓN DINÁMICA DIFERIDA`.
