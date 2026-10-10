# Tanda Validacion 128: conciliacion exhaustiva del historial local

Fecha: 2026-10-10

## Scope lock

Esta tanda inventaria y concilia exclusivamente los recibos ya persistidos en
`_nscbuilder3/history`. No cambia codigo, controladores, CMake, procesos,
producto, casos, manifiestos, oraculos ni bateria historica. No configura,
compila o ejecuta producto, no ejecuta baseline y no toca la unidad `C:`.

## Inventario completo

El mecanismo estructurado descubrio 17 directorios con `results.json`. Todos
poseen estado terminal legible: siete `PASSED`, diez `FAILED`, cero malformados
y cero desconocidos.

### Recibos exitosos

| Run | Operacion | Revision |
|---|---|---|
| `20261009-225838-013b3717` | `compiler-build` | `c9a1c52cef3ab404d8d8ad63c33fe63769ef263e` |
| `20261009-230646-f2ca1434` | `compiler-build` | `fe26a8ff23086466f2b86771cd1f0d5251e3c6cb` |
| `20261009-231217-72aa28dd` | `compiler-build` | `a88c1d7e08b7cfec55c35766162c76015d2d1726` |
| `20261009-232019-a2066666` | `compiler-build` | `fd1687c4ef3eaa98bab6278ea63e82b7f146b0ac` |
| `20261009-232620-c0db9e1d` | `tests-configure` | `4ec3d28d4ae4a9bbfa2ee75e3b81270d505240f7` |
| `20261009-234355-dbe63e3b` | `compiler-build` | `eaf2a9633d5f2bd1166d2532e68fc643d7676080` |
| `20261009-234407-ca01630f` | `tests-configure` | `eaf2a9633d5f2bd1166d2532e68fc643d7676080` |

Estos recibos acreditan solamente sus operaciones concretas; no sustituyen una
construccion y ejecucion integral posterior a todos los cierres.

### Recibos fallidos

| Run | Revision | SHA-256 de `full.log` | Grupo |
|---|---|---|---|
| `20261009-225634-de4956a7` | `ccdb175418972f3ad688bfb6984cda0ffa66b3fa` | `AB04F08B87FA94BF9AABA7628BB1A873D792BB0A9547120A7639EAE769AF72F8` | `L1` |
| `20261009-232044-83cdf91c` | `fd1687c4ef3eaa98bab6278ea63e82b7f146b0ac` | `166138D3A42A2C97C5E06D61E843C91CC09A64C7396215F51903C9E1D6EEA14D` | `L2` |
| `20261009-232622-a6b8658e` | `4ec3d28d4ae4a9bbfa2ee75e3b81270d505240f7` | `41E262CE3FE7D3221B7ED6A7419BE055D7FB1D94F3A845D1D11E2512E34A70AC` | `L3` |
| `20261009-232956-2fddd103` | `4ec3d28d4ae4a9bbfa2ee75e3b81270d505240f7` | `5319677BDA6CBCC99FC3AED4B3FC6A9B9347187B617C1B8E009DCDB5F3BF7A95` | `L4` |
| `20261009-233420-3f86baf0` | `4ec3d28d4ae4a9bbfa2ee75e3b81270d505240f7` | `25809B3B0A5756F8B7780492ACB5F9B743083BBDC2093F377A0709790F11C57A` | `L5` |
| `20261009-234342-2e6129db` | `eaf2a9633d5f2bd1166d2532e68fc643d7676080` | `9B7117BFCABEC3B8E612D47276C57994441FC101C4A15FE2960E003F5CBED0B0` | `L6` |
| `20261009-234348-0d4c195c` | `eaf2a9633d5f2bd1166d2532e68fc643d7676080` | `C6C9AD83C5BA8DD2F35DE602ACBCC5B7DC75FF20FEAC4F0E35A6D146EA47AC30` | `L6` |
| `20261009-234403-239b6165` | `eaf2a9633d5f2bd1166d2532e68fc643d7676080` | `578116BE44B88E6023CD5920A4DB6D205E5F38D84FA8106A455FC73C545E37BB` | `L6` |
| `20261009-234413-c33698bb` | `eaf2a9633d5f2bd1166d2532e68fc643d7676080` | `C6C9AD83C5BA8DD2F35DE602ACBCC5B7DC75FF20FEAC4F0E35A6D146EA47AC30` | `L6` |
| `20261009-234910-fcda0e5c` | `eaf2a9633d5f2bd1166d2532e68fc643d7676080` | `C6C9AD83C5BA8DD2F35DE602ACBCC5B7DC75FF20FEAC4F0E35A6D146EA47AC30` | `L6` |

Existen ocho logs fallidos unicos. Los runs `234348`, `234413` y `234910`
conservan exactamente el mismo `full.log`; no son tres causas distintas.

## Conciliacion causal

| Grupo | Causa comprobada | Cierre presente |
|---|---|---|
| `L1` | rutas Windows con `\` persistidas en `CMakeRCCompiler.cmake`, que producen `Invalid character escape '\c'` | la siguiente corrida `c9a1c52ce` configura y construye NSC; Validacion 28 fija rutas CMake normalizadas y árboles separados |
| `L2` | soporte Sema ausente y construcciones ambiguas en los modulos de prueba | `4ec3d28d4`; checkpoint de Validacion 121 |
| `L3` | structured binding por valor convertido en error por `/WX` | `9971289da`; Validacion 01 |
| `L4` | tipo MIR incompleto, API/opcode MIR inexistentes y parametros contractuales sin uso | `9971289da`; Validacion 01, cierre `523/523` unidades con `/Zs /W4 /WX` |
| `L5` | frontera privada de `SemanticAnalyzer` sin los productores de prueba autorizados | `eaf2a9633`; Validacion 02 |
| `L6` | identidad de producto y clausura transitiva de `WindowsLibraryBuilder` incompletas | `8524a263f` y `847af65c2`; Validaciones 03 y 04, con cero simbolos NativeSharp o LLVM-C sin productor en la auditoria predictiva |

Los cambios posteriores `171ad96e7`, `b4a1d8fe0`, `30ebdc793`, `08253c41a`,
`79a2a4daa` y `b73e42720` cerraron respectivamente autoridad runtime, consumo
CMake, cache LLVM-C, Python seleccionado, identidad del runtime y preservacion
del diagnostico. Ninguno posee todavia una corrida integral local posterior.

## Resultado del gate

```text
recibos inventariados: 17
recibos procesados: 17
recibos PASSED: 7
recibos FAILED: 10
logs fallidos unicos: 8
grupos causales: 6
grupos con cierre estatico presente: 6
grupos sin clasificar: 0
recibos malformados: 0
recibos omitidos: 0
```

## Limite de la afirmacion

El historial local queda inventariado y conciliado; no demuestra que el HEAD
actual compile o ejecute sin errores. Las causas antiguas no se presentan como
fallos actuales, pero tampoco se promueven a cierre funcional. La corrida
integral posterior al HEAD actual permanece `PENDING_USER_RUN`, y la tarea
persistente sigue activa hasta demostrar cero errores inesperados en la matriz
ejecutable acordada.
