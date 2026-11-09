# T08-D13: el alias NativeSharp de almacenamiento C `volatile` sigue el modo de su origen

Fecha: 2026-11-09. Decisión del usuario (T08-D13, opción 1 de T08-010-05a): el alias debe respetar el modo del
record o del encabezado de origen, validado frente al acceso directo.

## Contrato

El modo volatile es el `VolatileMode` de la `CppLibraryReference` que declara el almacenamiento C (Default es el
modo del destino, Microsoft en Windows). El acceso directo ya lo usaba; ahora también lo usa todo alias NativeSharp
derivado de ese almacenamiento. El proyecto consumidor no tiene modo propio.

**Dónde empieza el contrato C de un alias:**

| Origen | Modo | Alineación supuesta |
|---|---|---|
| Miembro flexible `volatile T items[]` que decae | el del record | la del campo, como C y el acceso directo `v->items[i]` |
| Campo `volatile T *` y arreglo de esos campos | el del record | la natural de `T` |
| Global `volatile T *`, arreglo de esas globales y `extern volatile T a[]` | el de la global (su referencia) | la natural de `T` |

**Cómo se propaga:**

- `var` local, alias de alias, aritmética `p + i` (alineación `gcd`) e indexación de un elemento puntero.
- Condicional entre dos alias: se unen los dos contratos.
- Reasignación, también en ramas: unión sin flujo sobre todas las asignaciones del local.
  - Si los modos discrepan o el valor no es alias C, el modo pasa a desconocido.
  - La alineación es la menor.

**Qué emite el HIR:**

- Un operando local usa el contrato unido de su local.
- Modo Microsoft con alineación demostrada: `load atomic volatile … acquire` y `store atomic volatile … release`,
  como Clang `-fms-volatile`.
- Modo ISO: accesos `volatile` simples, como Clang `-fno-ms-volatile`.
- Modo desconocido (referencias mixtas o reasignación entre modos): `volatile` simple, igual que el acceso directo
  a ese almacenamiento.
- Alineación no demostrada: sin atómico (T08-015-04e). Un miembro flexible empaquetado no tiene alias:
  `var p = v->items` se sigue rechazando.

## Cambios

**Análisis de cuerpos.**

- `ExpressionResult`, `MethodExpressionTypeBinding` y `MethodLocalSymbol` llevan el modo y la alineación del
  contrato.
- `CppVolatileAliasContract.h` (nuevo) calcula el ancho del escalar apuntado.
- Los analizadores de identificadores, miembros, declaraciones, asignaciones, aritmética, índices y condicionales
  inician, copian o unen el contrato.

**HIR.** `CppVolatileAliasAlignment.h` añade `cVolatileAliasAccess`. Las cargas, los almacenamientos y el acceso
indexado de `HirCompletionTerminalFrames.cpp` y `HirCompletionAccessFrames.cpp` lo usan en lugar del modo del
consumidor.

**Grafo de proyectos (T08-015-03c).** Dos proyectos vivos de un grafo pueden importar el mismo encabezado, cada uno
con su referencia y su modo:

- `IdentifierAnalyzer.cpp`: si una global C se declara en el proyecto que la usa y en uno referenciado, y solo
  difieren en el modo, el código del proyecto usa su propia declaración. Antes: «ambiguous namespace state
  reference». Sin declaración propia sigue siendo ambigua.
- `QualifiedTypeNames.cpp`: el mismo typedef de record del mismo encabezado en dos proyectos vivos (mismo USR del
  typedef y del record) nombra el record del proyecto dueño. Antes: «conflicting C/C++ type alias». Esto no depende
  del modo. Los typedef homónimos de otros records siguen siendo ambiguos (T08-D10).

**Propiedad `CppConsumerVolatileMode` (T08-015-03a, T08-015-08).** Con este contrato no tiene significado.
`ProjectCompilation.cpp` cambia la protección temporal («not yet supported by code generation») por un rechazo
definitivo: «CppConsumerVolatileMode is not supported: the volatile mode of C storage and of its aliases is the
VolatileMode of its CppLibraryReference». El cargador y el modelo de la propiedad no cambian; los fijan pruebas
unitarias históricas.

## Correcciones encontradas al validar

1. **Alias de miembro flexible.** Tomaba la alineación demostrada por el receptor, que es 1 para un `VFlex*` sin
   alineación declarada, y nunca era atómico. Ahora usa la del campo, como C y el acceso directo.
2. **Reasignación desde un miembro flexible.** `p = b->items` borraba el modo porque el valor no se reconocía como
   alias C. Ahora se reconoce, igual que en el inicializador.
3. **Grafo.** Las dos ambigüedades descritas arriba.

## Prueba nueva: `pruebas-tanda-08-volatile-alias-c/validar-alias-modo.py`

Cada caso es una función C y un método NativeSharp con el mismo cuerpo. El IR se compara con Clang por función: en
O1 en todos los caminos y en O0–O3 en el directo.

| Caso | Cuerpo |
|---|---|
| `Kflex` | `var p = v->items; p[0] = 1; *p = 2; return p[1];` |
| `Kchain` | alias de alias, `p + 1` |
| `Karith` | `*(p + i)` |
| `Kreassigned` | `p = a->items; …; p = b->items;` |
| `Kconditional` | `c != 0 ? pa : pb` |
| `Kbranches` | reasignación dentro de un `if` |
| `Kcompound` | `*p += 3; p[1] -= 1;` |
| `Kfield`, `Kelement` | campo `volatile int *` y elemento de `volatile int *pvs[2]` (alias y directo) |
| `Kglobal`, `Kgarray`, `Kgflex` | `volatile int *g_vp`, `g_pvs[2]`, `extern volatile int g_flex[]` |
| `Kdirect` | `v->items[0]` directo, la referencia |

**Resultados (todos aprobados):**

- **Directo:** Default y Microsoft iguales a `-fms-volatile` y todo acceso atómico; ISO igual a `-fno-ms-volatile`.
- **Precompilado consumido sin encabezado (T08-015-05c):** 70 de 70 accesos atómicos en Default y Microsoft, 0 en
  ISO.
- **Referencias mixtas** Microsoft + ISO en un proyecto (T08-010-07, T08-015-03d): todo `volatile` simple, igual
  que el acceso directo.
- **Grafo** (T08-015-03c), con typedef de records y solo con etiquetas:
  - biblioteca en modo Microsoft y ejecutable en ISO;
  - cada proyecto conserva su modo, también en las globales que declaran los dos;
  - el ejecutable se ejecuta en O0–O3;
  - el ejecutable importa el encabezado sin `Library`, porque la biblioteca C ya la enlaza el proyecto biblioteca.

  Con una copia propia de la misma biblioteca en cada proyecto, una de 61 construcciones falló en NSLink con
  NSL1021 «duplicate symbol definition: 'flex_a'». Las otras 60 pasaron (`claude\sonda-grafo-enlace2.py`), así que
  no se reprodujo. Queda registrado como observación; NSLink no se modifica.
- **Caché (T08-015-06a):** con `--artifact-cache`, cambiar la referencia de Microsoft a ISO produce un fallo de
  caché y repetir produce un acierto (`[False, True, False, True]`).
- **Ejecución en O0–O3**, Microsoft e ISO: el resultado coincide con el control Clang (185).
- **Rechazos:**
  - alias de miembro flexible empaquetado (T08-015-04e);
  - pasar el alias a un parámetro NativeSharp `int*`, que descartaría `volatile` (T08-015-05a);
  - la propiedad `CppConsumerVolatileMode` (T08-015-08).

**Diagnóstico histórico.** `flexible-c/diagnosticar-var-volatile.cmd` (T08-010/015) mostraba `DIFIERE` en Microsoft.
Ahora, en O0–O3, los casos directo, `var`, aritmética y reasignado son `IGUAL` en Microsoft y en ISO. El script
termina con 1 por diseño: compara las referencias mixtas con los dos modos de Clang, y el modo Microsoft siempre
difiere. Que difiera es el contrato.

**Prueba actualizada.** `arreglos-c/validar-arreglos-punteros-campo.py` (T08-D23, de esta tanda) esperaba
`store volatile` en modo Default. Ahora espera `store atomic volatile … release`, igual que Clang `-fms-volatile`:
directo por el elemento, por el alias `var` y por el campo del struct empaquetado (el dato apuntado está alineado).

## Pendientes funcionales

### T08-015-05a: alias que llega por el retorno de una función C

**Síntoma.** `var r = vp_get(); *r = 2;` con `volatile int *vp_get(void);`, en modo Microsoft, emite `volatile`
simple. Clang emite acquire/release.

| Caso | Microsoft NSC | Microsoft Clang | ISO NSC | ISO Clang |
|---|---|---|---|---|
| retorno de función C | (4, 0) | (2, 2) | (4, 0) | (2, 0) |
| alias pasado a parámetro C | (4, 4) | (2, 2) | (4, 0) | (2, 0) |

Sonda: `claude\sonda-retorno-c.py`. Cada celda es (accesos `volatile`, atómicos) en el módulo. NSC cuenta el doble
porque también incluye `Main` con `K` expandida.

**Causa.** Los símbolos de función C no llevan el modo de su referencia: lo tienen globales y records, pero no
funciones. El resultado de la llamada no inicia el contrato.

**Qué funciona.** Un alias pasado a un parámetro C funciona: el acceso lo hace C.

**Límite del lenguaje.** NativeSharp no puede escribir `T volatile*` en parámetros ni retornos propios, así que
el alias no cruza funciones NativeSharp. Ese paso se rechaza con «no viable overload».

**Alternativas.**

1. Llevar el modo de la referencia al símbolo de función C, en el inventario, en `CallAnalyzer` y en la instantánea
   de dependencias de la plantilla precompilada. Esto cambia el contenido del contrato NSMeta, no su versión.
2. Admitir `T volatile*` en firmas NativeSharp con el contrato C. Es un cambio de lenguaje.

### Condicional entre dos miembros flexibles

**Síntoma.** `var p = c ? a->items : b->items;` se rechaza con «operand of * must be a non-void pointer». C lo
acepta.

**Causa.** El condicional no hace decaer los dos arreglos. Es independiente del modo. Con alias intermedios
(`c ? pa : pb`) funciona.

## Validación

- **`claude\scripts\regresion-d13.cmd`**: todo correcto salvo el diagnóstico histórico, que termina con 1 por
  diseño. En la segunda ejecución, `validar-alias-modo.py` falló una vez por el NSL1021 descrito arriba, cuando el
  ejecutable del grafo todavía enlazaba su propia copia de la biblioteca; con el cambio pasó. Cubre:
  - `volatile-alias-c`;
  - `record-c` (`validar-miembros`, `validar-mascara-bits`, `validar.cmd`);
  - `calificadores-c`, `layout-comparacion-c`;
  - `arreglos-c` (los dos), `punteros-c`;
  - `colisiones-c` (los dos);
  - `typedef-c` (productores, dos encabezados, reconciliación);
  - `globales-c` (escalares calificados, atómicas y atómicas de record);
  - `union-c`, `layout-c/validar-alineacion-efectiva`;
  - `flexible-c` (`validar`, `validar-decay`), `volatile-campos-c`, `volatile-punteros-c`;
  - `bitfields-c` (seis validadores);
  - `global-puntero-const`, `identidad-c/typedef-funcion-record`.
- **ctest:** pasan `nsc.cpp-interop.session`, `nsc.compilation-model`, `nsc.project-loader` y `nsc.project-graph`.
- **`nsc.project-oop-lowering`:** falla en 25 de 660 comprobaciones (agregados, `With`, spread, `EnumToString`, el
  fixture de operador unario). Falla igual sin los cambios de D13: se comprobó guardando `src` e `include` con
  `git stash`, reconstruyendo y restaurando (`claude\scripts\base-lowering.cmd`). Es un fallo previo y ajeno.
