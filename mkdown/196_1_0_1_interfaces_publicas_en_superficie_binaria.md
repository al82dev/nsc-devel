# 1.0.1 — Interfaces públicas en la superficie binaria

## Resultado

Las firmas públicas que reciben o devuelven una interfaz pública se publican
como parte del contrato precompilado. NSMETA, la tabla de exports de una DLL y
el verificador de artefactos vuelven a describir la misma superficie física.

## Causa raíz

`canMaterializeContractType()` reconocía clases, estructuras, enumeraciones y
delegates, pero omitía `LibraryMetadataTypeKind::Interface`. Como consecuencia,
`canMaterializeNativeMethod()` descartaba de la superficie publicada cualquier
método cuyo retorno o parámetro fuera una interfaz, aunque la interfaz fuera
pública y tuviera un carrier nativo válido.

La generación PE evaluaba la accesibilidad pública desde el modelo del
frontend y sí exportaba esos métodos. El resultado era una DLL con símbolos
legítimos que `--verify-library-artifacts` clasificaba como exports inesperados
porque el cálculo basado en NSMETA los había omitido.

## Archivos e implementación

- `src/Driver/Driver.cpp`, en `canMaterializeContractType`, admite
  `LibraryMetadataTypeKind::Interface` como referencia nativa opaca.
- La interfaz sólo se considera materializable cuando:
  - su accesibilidad es `public`;
  - toda su cadena de tipos contenedores es públicamente alcanzable;
  - su tamaño y alineación de referencia coinciden con el puntero del ABI;
  - la firma no intenta materializar un arreglo de interfaces.
- Se conserva sin cambios la política existente para clases, structs, enums y
  delegates.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp` añade una biblioteca DLL con un
  método público que usa la misma interfaz pública como parámetro y retorno. La
  regresión exige que la DLL, su import library y NSMETA pasen juntos la
  verificación de artefactos.

No se modificaron SaruGL, bindings, tutoriales ni repositorios externos.

## Por qué se resolvió aquí

La exportación PE ya reflejaba la API pública correcta. El punto divergente era
la clasificación de tipos materializables usada para reconstruir y verificar
NSMETA. Corregir esa regla central evita mantener dos listas especiales de
símbolos y alinea también futuros consumidores precompilados.

## Verificación prevista

La regresión queda guardada para certificación posterior. En este carril sólo
se compilará e instalará NSC; no se ejecutarán SaruGL, bindings, tutoriales ni
baseline.

## Commit de referencia

### Español

**Título:** Publica firmas con interfaces en NSMETA

**Descripción:** Reconoce interfaces públicas y públicamente alcanzables como
referencias materializables en parámetros y retornos. Alinea la superficie
requerida por NSMETA con los exports producidos por DLL y añade una regresión
para ambos sentidos de la firma.

### English

**Title:** Publish interface signatures in NSMETA

**Description:** Recognize public, publicly reachable interfaces as
materializable references in parameter and return positions. Align the NSMETA
required surface with DLL exports and add regression coverage for both sides
of the signature.

## English summary

Public interface types are now valid precompiled contract carriers. Methods
that accept or return them are published consistently by NSMETA and the DLL
export pipeline, eliminating false unexpected-export failures.
