# NS3-T02-F05 — Campos promovidos / Promoted fields

**Estado / Status:** TERMINADA / COMPLETE
**Commit:** `ns3(t02-f05): promoted-fields`

## Resultado / Outcome

La implementación productiva histórica fue auditada y cerrada como contrato independiente. El
parser desazucara cada parámetro primario a un campo físico ordinario y construye un constructor
ordinario que asigna `this.Field = Field` en orden textual. No existe una captura escondida ni una
ruta paralela en HIR, MIR, LLVM o ABI.

The historical production implementation was audited and closed as an independent contract. The
parser desugars every primary parameter into an ordinary physical field and builds an ordinary
constructor that assigns `this.Field = Field` in textual order. There is no hidden capture or
parallel HIR, MIR, LLVM, or ABI path.

## Cobertura dirigida / Directed coverage

- ejecución O0 con NSLink local y resultado `42`;
- evaluación izquierda a derecha y una asignación por campo;
- defaults, `readonly`, lista vacía y defaults de visibilidad distintos para `struct`/`record`;
- especialización genérica;
- orden físico, offsets, tamaño y alineación Windows x64;
- nombres del tipo, campos y parámetros presentes en el objeto CodeView;
- rechazo de duplicados, shadowing, default requerido después de opcional, `protected`, acceso
  privado, escritura `readonly` y ownership primario aún inseguro.

The dedicated test covers O0 execution through local NSLink, left-to-right evaluation, defaults,
readonly storage, empty lists, visibility defaults, generic specialization, Windows x64 layout,
CodeView names, and every listed negative boundary.

## Archivos / Files

### Nuevos / New

- `tests/unit/NativeSharp3T02PromotedFieldTests.cpp`: frontera exclusiva de 11 comprobaciones.
- `docs/3.0/language/promoted-fields.md`: contrato bilingüe y desazucarado completo.
- este documento de evidencia.

### Modificados / Modified

- `CMakeLists.txt`: target/test `nsc_ns3_t02_promoted_field_tests`.
- documentos de cierre de T02 para separar F05 de F04 y F06.

No se creó una segunda implementación: la mejora consiste en convertir el camino productivo ya
existente en un contrato comprobable y estable, en vez de duplicarlo detrás de otro helper.

## Validación / Validation

- target exclusivo compilado en `A:\nsc-devel\build-test-nslink`;
- `nsc_ns3_t02_promoted_field_tests.exe`: `PASS: 11 checks`;
- sólo O0 para programas NativeSharp y `A:\nsc-devel\build-test-nslink\bin\nslink.exe`;
- sin baseline, certificación, O1/O2/O3, instalación, deploy ni escritura en `C:`.
