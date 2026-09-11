# 1.0.1 — Accesibilidad efectiva de símbolos NSMETA

## Resultado

El verificador de artefactos sólo exige el símbolo físico de un campo estático
cuando el miembro y toda su cadena de tipos contenedores son públicamente
accesibles. Un campo declarado `public` dentro de una clase `internal` continúa
siendo un detalle del productor y no forma parte de la API requerida.

## Causa raíz

`publishedNativeSymbols()` ya comprobaba accesibilidad efectiva para métodos,
constructores y accessors de descriptors. Su rama independiente para campos
estáticos sólo consultaba `member.accessibility`, el modificador `static`, la
ausencia de `const` y el nombre físico.

Esto hacía que campos como `VertexSkinData.Rigid` y `SilkKeyTable.AllKeys`
fueran exigidos a `SaruGL.lib`, aunque sus clases contenedoras fueran
`internal` y no pudieran ser nombradas por un consumidor.

## Archivos e implementación

- `src/Driver/Driver.cpp`, en `publishedNativeSymbols`, resuelve
  `member.declaringTypeKey` y exige:
  - tipo contenedor presente;
  - accesibilidad `public` del contenedor;
  - `isPubliclyReachableMetadataType`, que recorre también contenedores de tipos
    anidados.
- Las reglas existentes para miembros, `static`, `const` y símbolo físico no
  cambian.
- Los exports C explícitos permanecen requeridos porque pertenecen al ABI del
  host, independientemente de la superficie nominal NativeSharp.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp` añade una clase `internal` con
  un campo `public static`. La fixture sustituye el nombre físico por uno
  ausente y exige que `--verify-library-artifacts` siga aceptando el archivo.

No se modificaron SaruGL, bindings ni scripts externos.

## Verificación prevista

La regresión queda guardada para certificación posterior. En este carril sólo
se compilará e instalará NSC; no se ejecutarán SaruGL, bindings, tutoriales ni
baseline.

## Commit de referencia

### Español

**Título:** Respeta el contenedor al exigir símbolos NSMETA

**Descripción:** Excluye de la superficie física requerida los campos públicos
de tipos internos o no alcanzables públicamente. Conserva las comprobaciones
actuales para campos de contenedores realmente públicos y para exports C.

### English

**Title:** Honor declaring type when requiring NSMETA symbols

**Description:** Exclude public fields of internal or publicly unreachable
types from the required physical surface. Preserve existing checks for fields
of genuinely public containers and for C exports.

## English summary

Required NSMETA symbols now use effective accessibility rather than the member
modifier alone. Public fields hidden by an internal declaring type no longer
produce false missing-symbol errors.
