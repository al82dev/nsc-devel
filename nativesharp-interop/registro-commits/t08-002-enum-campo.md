# T08-002: enum declarado dentro de un campo de record C

Reproducción de extremo a extremo del hallazgo H4 con el fixture
`pruebas-tanda-08-identidad-c/enum-campo` (diez casos en `casos.txt`):

- Con el encabezado directo, el campo de tipo enum anónimo se omite y los
  enumeradores declarados dentro del struct no existen.
- Un enum con nombre declarado dentro del struct no se importa.
- Por biblioteca precompilada, el productor falla en los dos casos.

La sonda 6 nueva de `nsc_cpp_c_identity_tests` afirma la identidad que da
Clang 23.1.0:

- Para el enum con nombre, `c:@E@<nombre>`, que no depende del encabezado.
- Para el enum anónimo, `c:@SA@<record>@E@<archivo>@<desplazamiento>`, que sí
  depende del encabezado, y un nombre calificado con la ruta absoluta.
- Solo el USR del campo es estable.

Se añade `T08-002-PROPUESTA-ENUM-CAMPO.md` con causas, casos y archivos
afectados. La parte A recorre los records C al capturar enums y la parte B
proyecta el campo con su entero subyacente; ninguna de las dos cambia la
metadata. La parte C añade una entrada de enum anónimo de campo al catálogo,
cambia la metadata y no se implementa.

`validar.cmd` sustituye la observación anterior por `:enum_field`, con las
subrutinas `:prepare_cases` y `:cases`. Se elimina
`anonimos/ProducerAnonLevel.ns`, que cubre `enum-campo/PAnon`.
