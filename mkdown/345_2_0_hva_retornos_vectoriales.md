# Ítem 345 — HVA y retornos vectoriales / HVA and vector returns

## Resultado / Result

La frontera `extern vectorcall delegate` clasifica `Float32x4` como `<4 x float>` directo y los
structs homogéneos de uno a cuatro campos `Float32x4` como HVA directos. Parámetros y retornos usan
registros vectoriales; un HVA de parámetro publica además `inreg`, igual que Clang para Microsoft
x64. / The `extern vectorcall delegate` boundary classifies `Float32x4` as a direct `<4 x float>`
and homogeneous structs of one to four `Float32x4` fields as direct HVAs. Parameters and returns
use vector registers; an HVA parameter also publishes `inreg`, matching Clang on Microsoft x64.

## Puente interno / Internal bridge

El ABI ordinario y el cuerpo NativeSharp no cambian: conservan el carrier agregado y las reglas
puntero/sret. El thunk protegido convierte cada argumento vectorial al carrier interno, invoca el
cuerpo con la convención ordinaria y convierte el resultado de vuelta a la forma vectorcall. Los
unions, layouts explícitos, `pack`, campos mixtos y agregados de más de cuatro vectores no se
clasifican como HVA. / The ordinary ABI and NativeSharp body do not change: they retain the
aggregate carrier and pointer/sret rules. The guarded thunk converts each vector argument to the
internal carrier, invokes the body with the ordinary convention, and converts the result back to
vectorcall form. Unions, explicit layouts, packing, mixed fields, and aggregates over four vectors
are not classified as HVAs.

## Pruebas / Tests

La prueba focalizada ejecuta ida y vuelta de `Float32x4` y de un HVA de dos miembros en O0/O2, con
resultado 42. También exige en IR el retorno `<4 x float>`, el HVA `{ <4 x float>, <4 x float> }`,
`inreg` y ambos puentes de representación. / The focused test executes `Float32x4` and a two-member
HVA round trip at O0/O2 with result 42. It also requires the `<4 x float>` return, the
`{ <4 x float>, <4 x float> }` HVA, `inreg`, and both representation bridges in IR.
