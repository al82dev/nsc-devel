# T08-010-04 y T08-010-06: miembros de struct C

Fecha: 2026-11-08. Casillas: T08-010-04a–f y T08-010-06a–d.

## Resultado

Verificadas con tres correcciones en el compilador:

1. Campos puntero con apuntado `volatile` (04b).
2. Máscara de escritura de campos de bits (04d).
3. Campos de bits cuya ventana mínima cruzaba el final del record (04d).

T08-010-05, 07, 08 y 09 quedan bloqueadas por la decisión pendiente T08-010-05a (ver el tracking).

## Correcciones

### 1. Campo C `volatile T *` (04b)

**Síntoma.** El campo no se importaba:

- en el camino directo, «receiver/type '…' has no compatible member named 'vp'»;
- en el precompilado, «C record field cannot be reconstructed».

**Causa.** `projectCppSemanticInstanceField` excluía todo apuntado `volatile`. Además, el campo importado
solo podía guardar `pointeeReadonly`.

**Corrección.**

- En records C, `CppSemanticInstanceField` y `CppImportedInstanceField` llevan ahora `pointeeVolatile`.
- `ProjectMemberModel` lo copia a `declaredPointerPointeeVolatile`, de modo que quitar la calificación se
  rechaza igual que en globales y locales.
- En records C++ la exclusión se mantiene.

**Orden de acceso.** Al desreferenciar o indexar el puntero leído del campo (`*s.vp`, `p->vp[i]`, `*s.vp += k`),
el HIR toma el modo volatile del record. Lo hace con `cFieldVolatilePointeeMode` en
`CppVolatileAliasAlignment.h`, igual que ya ocurría con las globales C `volatile T *`.

Con el modo Microsoft (y el modo por defecto de este target) el IR emite `load atomic volatile … acquire` y
`store atomic volatile … release`, como Clang con `-fms-volatile`. Con ISO emite accesos `volatile` simples,
como Clang con `-fno-ms-volatile`. Si dos referencias del record discrepan, el modo queda desconocido y el
acceso es simple, como en el acceso directo existente.

### 2. Máscara de preservación en campos de bits (04d)

**Síntoma.** NSC se detenía en -O1 (código 0x80000003) al compilar `b.c = 127; b.d = 0;` sobre
`unsigned c : 7; int d : 1`. El backend imprimía `MOV8mi … i8 127`. El fallo también ocurría en un consumidor
sin encabezado, es decir, sin sesión de Clang.

**Causa.** `insertBitField` pasaba `~fieldMask`, un complemento de 64 bits, a `LLVMConstInt` con el tipo
estrecho de la unidad de asignación. LLVM no trunca ese valor y la constante queda con bits altos sobrantes,
es decir, no canónica. Su texto es idéntico al de una constante canónica: `llc` sobre el IR volcado no falla,
pero el backend en memoria sí.

**Corrección.** La máscara se recorta al ancho de la unidad (`~fieldMask & storageMask`).

### 3. Ventana de campos de bits que cruza el final del record (04d)

**Síntoma.** Un campo como `unsigned hi : 24` después de un byte de campos de bits, o
`unsigned long long c : 50` en el bit 13, no se importaba: «has no compatible member named».

**Causa.** La ventana de acceso empezaba siempre en el primer byte del campo y crecía en potencias de dos.
Para esos campos terminaba fuera del record.

**Corrección.** Si esa ventana no cabe, se usa la ventana alineada naturalmente más pequeña que contiene el
campo, que es la unidad de asignación por la que lo lee Clang. Los campos que ya funcionaban no cambian.

## Pruebas

### `pruebas-tanda-08-record-c/validar-miembros.py` (nueva)

Aprobada en O0–O3, con rechazos en O0.

**Mecanismo.** El mismo cuerpo se ejecuta en:

- un control Clang;
- el camino directo;
- un productor precompilado consumido con el encabezado (06b);
- un productor precompilado consumido solo con metadata (06c).

Para cada struct se comparan `sizeof` y `offsetof` con Clang. C llena objetos que NativeSharp lee, y
NativeSharp escribe valores que C comprueba.

**Casillas cubiertas.**

- **04a:** enteros de todos los anchos, `float`, `double`, enum y `_Bool`.
- **04b:**
  - punteros a `int`, `const int`, `volatile int` y `volatile void`;
  - puntero `const`;
  - puntero a función, por typedef y directo.
- **04c:** arreglo fijo y miembro flexible.
- **04d:** campos de bits con y sin signo, anónimos y de ancho cero.
- **04e:** promoción de miembros de struct y union anónimos en tres niveles. Cubre también 06a: lectura y
  escritura cruzadas en el camino directo.
- **04f:** struct anidado en `pack(1)`, `pack(2)` y `pack(4)`.
- **06d:** miembros flexibles `const` y `volatile` en los tres caminos.

**Rechazos.** Clang rechaza todos salvo `quitar-volatile-campo`, que en C solo produce un aviso.

| Caso | Diagnóstico de NativeSharp |
| --- | --- |
| Escribir un puntero `const` | «left side of assignment is not assignable» |
| Escribir a través de `const int *` | «left side of assignment is not assignable» |
| Quitar `volatile` de un campo | «initialization would discard pointer pointee volatile qualification» |
| Tomar la dirección de un campo de bits | «bit fields have no address» |
| Escribir un miembro flexible `const` | «left side of assignment is not assignable» |

**Control de IR (04b).** Compara el IR de Clang `-fms-volatile` y `-fno-ms-volatile` con el de NSC en los modos
Default, Microsoft e ISO. Lo hace en el camino directo y en una plantilla precompilada consumida sin
encabezado. Los accesos cubiertos son lectura, escritura, `+=` e indexación.

### `pruebas-tanda-08-record-c/validar-mascara-bits.py` (nueva)

Aprobada en O0–O3, en el camino directo y en una plantilla precompilada sin encabezado.

Escribe valores extremos en campos con unidades de 8, 16, 32 y 64 bits, junto a vecinos en cero y saturados, y
C comprueba cada patrón. Incluye los dos campos de la corrección 3.

Con el compilador anterior:

- el caso original de `validar-miembros.py` (`Bits.c = 127; Bits.d = 0`) se detenía en -O1;
- los campos `hi` y `c` de `validar-mascara-bits.py` no se importaban.

### Regresión

Ejecutada con el compilador final. La lista completa está en el commit.

- `volatile-campos-c`, `volatile-punteros-c`.
- `flexible-c` (`validar` y `validar-decay`).
- `record-c/validar.cmd`.
- `bool-campos-c`, `bool-puntero-c`.
- `global-puntero-const`.
- `anonimos-c`.
- `bitfields-c` (`validar`, `validar-volatile`, `validar-volatile-metadata`, `validar-matriz-volatile`,
  `validar-metadata` y `validar-union`).
- `void-c`, `globales-c/validar-enum-record.py`.
- La regresión de enum, la de typedef y la de identidad.

## Decisión pendiente T08-010-05a

**Pregunta.** ¿Qué modo volatile rige el acceso por un alias NativeSharp derivado de almacenamiento C
`volatile`, como `var p = vf->items; *p = 1;` o un parámetro `int volatile*` que recibe un campo C?

**Opciones.**

1. El modo del record o encabezado de origen, como en el acceso directo, sea a campo, global o al puntero
   leído de un campo.
2. Un modo propio de la unidad consumidora. Esa propiedad ya existe: `CppConsumerVolatileMode` está modelada,
   pero sigue bloqueada en el HIR.

Las dos opciones producen IR distinto (acquire/release frente a accesos simples) cuando las referencias o el
proyecto difieren. Ya hay evidencia de que el alias pierde el orden en modo Microsoft:
`t08-flexible-var-modos-mixtos-resultado.md` y `t08-volatile-consumidor-ruta.md`.

**Casillas afectadas.** De esa decisión dependen T08-010-05, 07, 08 y 09.
