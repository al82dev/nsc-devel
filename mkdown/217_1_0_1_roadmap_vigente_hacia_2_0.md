# 217 — Roadmap vigente de NativeSharp 1.0.1-dev hacia 2.0

## 1. Objetivo / Objective

**ES.** Convertir los planes históricos de madurez y los handovers de DLL/PDB en una sola guía
vigente para la línea `1.0.1-dev`, su promoción, la evolución compatible 1.x y la entrada
deliberada a 2.0.

**EN.** Convert the historical maturity plans and DLL/PDB handovers into one current guide for the
`1.0.1-dev` line, its promotion, compatible 1.x evolution, and deliberate entry into 2.0.

## 2. Archivos creados / Created files

### `docs/1.0.1/ROADMAP_TO_2_0.md`

Documento público, instalable y vigente que define:

- la dirección nativa del producto y sus exclusiones;
- el inventario reconciliado de lo ya implementado;
- la política de snapshots `nativesharp-1.0.1-dev-NNN`;
- el cierre de 1.0.1 y las reglas de promoción;
- el contrato compatible de la línea 1.x;
- la puerta que determina cuándo un cambio realmente necesita 2.0;
- las fases 2.0-A a 2.0-G;
- disciplina de pruebas, propiedad entre repositorios y riesgos;
- el siguiente hito recomendado después de `dev-000`.

### `mkdown/217_1_0_1_roadmap_vigente_hacia_2_0.md`

Registro histórico de esta entrega, sus decisiones, comprobaciones y mensaje de commit.

## 3. Archivos modificados / Modified files

### `docs/1.0.1/README.md`

Agrega el roadmap a la ruta de lectura oficial inmediatamente después del contrato de versión y
renumera los puntos posteriores. El roadmap queda así incluido automáticamente en el árbol
documental que CMake instala bajo `share/nativesharp/docs/1.0.1`.

## 4. Funciones e implementación / Functions and implementation

Esta entrega no modifica funciones, clases, ABI, metadata ni código ejecutable. Su implementación
es documental:

1. toma como punto inicial el snapshot local `nativesharp-1.0.1-dev-000` en `5ff40d8a`;
2. reconcilia `chatgpt_110_plan_continuacion_madurez_nativesharp.md` con el estado actual;
3. incorpora el cierre P1–P6 de bibliotecas registrado por `DLL_HANDOVER_AND_ROADMAP.md`;
4. incorpora el cierre del productor PDB y sus pendientes de consumo/linker;
5. distingue implementación, snapshot, certificación y promoción;
6. reserva SemVer major para rupturas reales con migración documentada;
7. adopta O0 y pruebas dirigidas como iteración ordinaria;
8. mantiene compilación completa y baseline como acciones separadas y explícitas.

## 5. Por qué se implementó así / Why it was implemented this way

Los planes anteriores fueron escritos antes de varias entregas que hoy ya existen: NSMETA 1.12,
clausura genérica precompilada, verificación física de bibliotecas, instalación reproducible y la
superficie CodeView/PDB. Copiarlos literalmente habría vuelto a planear trabajo cerrado.

El roadmap nuevo no enumera características de C# como deuda. Evalúa trabajo por consumidor,
contrato, natividad, costo, evidencia y compatibilidad. También evita usar 2.0 como nombre informal
para cualquier mejora: una versión major requiere una ruptura necesaria y una estrategia de
migración.

The previous plans predated major delivered work. The new roadmap therefore reconciles rather than
copies them, separates current implementation from certification, and prevents ordinary compatible
work from being mislabeled as a 2.0 requirement.

## 6. Decisiones principales / Main decisions

- `development` continúa siendo la rama de trabajo.
- Los snapshots `dev-NNN` son puntas inmutables por convención y no ramas de desarrollo.
- Crear un snapshot no ejecuta compilación, instalación ni baseline.
- El baseline sólo ocurre por instrucción explícita.
- Los defectos P0 de consumidores pueden reordenar el trabajo inmediato.
- NSC no modifica repositorios de debugger, linker, bindings o frameworks para ocultar defectos.
- 1.x conserva compatibilidad salvo correcciones de corrupción o miscompile.
- 2.0 sólo se abre con un libro mayor de rupturas y migración.
- Debug extremo permanece como contrato transversal del producto nativo.
- Self-hosting, GC, reflection y paridad general con C# no son gates.

## 7. Verificación / Verification

Comprobaciones documentales previstas para esta entrega:

- los enlaces relativos agregados al índice resuelven dentro de `docs/1.0.1`;
- el snapshot y commit citados existen;
- no se cambia la versión del compilador ni ningún contrato binario;
- el diff queda limitado al roadmap, el índice y este registro;
- no se compila el producto y no se ejecuta baseline.

## 8. Límites / Limits

- El documento no promueve 1.0.1 ni certifica `dev-000`.
- No crea todavía `dev-001`.
- No asigna fechas artificiales a 2.0.
- No autoriza automáticamente las características candidatas de las fases.
- No sustituye la especificación, el contrato del compilador ni el contrato PDB.

## 9. Mensaje de commit / Commit message

### Título ES / Spanish title

`Define el roadmap vigente de NativeSharp hacia 2.0`

### Title EN / English title

`Define the current NativeSharp roadmap toward 2.0`

### Descripción ES / Spanish description

`Reconcilia la línea 1.0.1-dev con los cierres de bibliotecas y PDB, formaliza los snapshots dev-NNN, separa iteración de certificación y establece la puerta de compatibilidad, las fases y los criterios de migración para una futura versión 2.0.`

### Description EN / English description

`Reconcile the 1.0.1-dev line with the library and PDB closures, formalize dev-NNN snapshots, separate iteration from certification, and establish the compatibility gate, phases, and migration criteria for a future 2.0 release.`
