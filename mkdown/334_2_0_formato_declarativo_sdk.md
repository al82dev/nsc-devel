# Ítem 334 — Formato declarativo de SDK / Declarative SDK format

## Resultado / Result

NativeSharp incorpora manifiestos `.nssdk` XML con identidad y versión explícitas. El formato 1
usa la raíz `NativeSharpSdk`, exige `Name` y `FormatVersion="1"`, limita cada archivo a 4 MiB y
rechaza atributos o extensiones desconocidas. Los proyectos los consumen con `SdkReference`; las
rutas se canonizan, ordenan y deduplican antes de entrar al modelo de compilación. / NativeSharp
now supports XML `.nssdk` manifests with explicit identity and version. Format 1 uses the
`NativeSharpSdk` root, requires `Name` and `FormatVersion="1"`, limits each file to 4 MiB, and
rejects unknown attributes or extensions. Projects consume them through `SdkReference`; paths are
canonicalized, sorted, and deduplicated before entering the compilation model.

## Contrato evolutivo / Evolution contract

El cuerpo queda reservado en el ítem 334 para que cada ampliación posterior tenga semántica y
pruebas propias. Una versión futura no se interpreta parcialmente: falla cerrada con un diagnóstico
estable. / Item 334 reserves the body so every later extension receives its own semantics and
tests. A future version is never partially interpreted: it fails closed with a stable diagnostic.

## Pruebas / Tests

La prueba dirigida cubre carga válida, identidad, versión, ruta canónica, rechazo de versión futura
y cuerpo desconocido, integración con `.nsproj` y referencias duplicadas. / The focused test covers
valid loading, identity, version, canonical paths, rejection of future versions and unknown bodies,
`.nsproj` integration, and duplicate references.
