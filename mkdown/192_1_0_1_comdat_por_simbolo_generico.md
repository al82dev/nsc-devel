# 1.0.1 — COMDAT por símbolo físico genérico

## Resultado

Cada definición genérica reutilizable se deduplica mediante un COMDAT propio,
identificado por su símbolo físico. Productor y consumidor pueden materializar
subconjuntos diferentes de una misma especialización sin que LLD descarte
constructores, métodos, descriptors o datos que todavía tienen referencias.

## Regresión corregida

La primera política COMDAT usaba una sola clave para toda la especialización.
Con `SelectionAny`, LLD podía elegir el grupo parcial del consumidor y descartar
miembros presentes únicamente en el productor estático. Las relocations del
productor quedaban apuntando a secciones descartadas.

El problema no era `weak_odr`: era la granularidad incorrecta del grupo.

## Archivos e implementación

- `src/LLVM/LlvmCApi.h` declara `LLVMGetValueName2`, la API oficial usada para
  leer la identidad física ya asignada a una función o global.
- `src/LLVM/ProjectOopLlvmLowerer.cpp` conserva `weak_odr`, pero deriva la clave
  COMDAT del nombre físico de cada entidad y mantiene un mapa por símbolo.
- Cada símbolo obtiene su propio líder neutro `i8`. Sólo ese símbolo queda
  asociado al líder; ya no existe dependencia entre miembros distintos de la
  especialización.
- El hash del nombre físico estabiliza el nombre COFF del líder sin depender de
  rutas, orden de materialización ni módulo consumidor.

La política se aplica uniformemente a funciones, almacenamiento estático,
accessors, destrucción, vtables, tablas de interfaces, descriptors y nombres
RTTI que ya estaban marcados como definiciones genéricas ODR.

No se modificaron bindings ni tutoriales.

## Verificación prevista

La aceptación dirigida es que ambos consumidores del productor estático
`precompiled_closed_generic_surface` enlacen: el que llama al método publicado
no debe producir relocations contra secciones descartadas y el que materializa
localmente debe continuar sin símbolos duplicados.

No se ejecutará baseline.

## Commit de referencia

### Español

**Título:** Separa el COMDAT de cada símbolo genérico

**Descripción:** Deriva cada grupo COMDAT del nombre físico de su función o
global, en lugar de compartir un grupo para toda la especialización. Permite
materialización parcial entre productor y consumidor sin descartar miembros
todavía referenciados.

### English

**Title:** Split COMDAT by generic physical symbol

**Description:** Derive each COMDAT group from the physical function or global
name instead of sharing one group across the entire specialization. Allow
partial producer and consumer materialization without discarding still
referenced members.

## English summary

Generic ODR definitions now use one COFF COMDAT per physical symbol. This keeps
deduplication while making independently materialized specialization subsets
safe to combine.
