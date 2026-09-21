# Tanda 48 — SH46: ABI e interop completos

| Campo | Valor |
|---|---|
| Estado | `STAGED-COMMITTED` — gate dirigido y baseline acumulado aprobados; integración lineal pendiente de SH44 |
| Base | T47/SH45 `ea6c15d86` en pila aislada `t48-staging` |
| Gate | SH46 |
| Instalación | `A:\native-sharp` |
| LLVM | `C:\clang-llvm` (sólo lectura) |

## Objetivo y contrato

Cerrar las fronteras nativas de NativeSharp 2.0 en el compilador autocontenido: C, C++, callbacks,
vtable COM, agregados por valor, varargs, vectorcall, EXE y DLL. La matriz debe ejecutarse en O0–O3
con LLD y NSLink, y una DLL producida por cada linker debe ser consumida desde C mediante su import
library real.

## Diseño

`LlvmBackend.AggregateSize()` distingue `struct`/`union` de clases y obtiene su tamaño del modelo
semántico. En la ABI Win64, agregados de 1, 2, 4 u 8 bytes se representan en la firma externa como
un entero del mismo ancho. `AbiValueType()` aplica esa representación exclusivamente a símbolos
`extern C`; la ABI interna NativeSharp continúa usando su representación nominal y no cambia.

En entrada, el entero físico se almacena en memoria local y el cuerpo conserva una dirección de
valor normal. En llamadas, el puntero nominal se carga con el ancho ABI; en retornos se realiza la
operación inversa. Los resultados de llamadas externas se materializan en un temporal direccionable,
por lo que el acceso posterior a campos conserva la semántica del lenguaje.

Los argumentos posteriores a los parámetros fijos de una función variádica aplican las promociones
C: `float → double` y enteros menores de 32 bits → `int` con la extensión de signo correcta.
`vectorcall` conserva calling convention 80 tanto en la función como en la instrucción de llamada.

La dirección de un agregado local dejó de exponer el slot interno que contiene su puntero; `&value`
devuelve ahora la dirección de sus bytes. Esta corrección es necesaria para entregar a C un objeto
con vtable y evita un nivel de indirección espurio.

## Archivos y funciones

| Archivo | Cambio |
|---|---|
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | ABI externa de agregados, promociones vararg y dirección de agregados |
| `selfhost/contracts/interop-parity.contract.json` | perfiles, linkers, símbolos, productos y fronteras obligatorias |
| `selfhost/bootstrap/verify_interop_parity.py` | compilación C/C++, enlace, DLL, consumo y ejecución |
| `cmake/VerifyNativeSharpInteropParity.cmake` | gate reproducible SH46 |
| `selfhost/tests/interop-2.0/Main.ns` | productor/consumidor NativeSharp y vtable COM |
| `selfhost/tests/interop-2.0/host.c` | agregados, callback, COM, varargs y vectorcall |
| `selfhost/tests/interop-2.0/host.cpp` | frontera C++ con thunk C |
| `selfhost/tests/interop-2.0/dll_consumer.c` | consumidor externo de exports NativeSharp |

Métodos nuevos o modificados: `AggregateSize`, `DirectAggregateSize`, `DirectAggregateType`,
`AbiValueType`, `ParameterDeclarationAt`, `UsesNativeAbi`, `ScalarFunctionType`,
`EmitScalarExpression`, `EmitStatement` y `EmitFunctionBodies`.

## Evidencia dirigida

- SH41: 101 fuentes, 16 ejecuciones, O0–O3, LLD/NSLink, objetos `nsc2/nsc3` idénticos 4/4;
- cuatro productores autocontenidos `nsc2` O0–O3;
- cada productor emitió los cuatro perfiles y completó EXE+DLL con LLD y NSLink;
- 64/64 ejecuciones aprobadas, cero fallos y cero omitidos;
- 47/47 pruebas de infraestructura;
- NSLink necesitó cero cambios: su soporte existente de DLL/export/import library quedó ejecutado.

SHA-256 fuentes SH41: `405366b82e945ab5c07ee1745b45d3c8e5acf95e639c49fe7f39afeb5f023c5a`.
SHA-256 contrato: `df2e5e717c3ecfba069ba3caedd23ded3b5bfa742afe2ca70f122e00ee4fad65`.
SHA-256 verificador: `4858478753df21a5bf5b37143817917c671d8d1988f8a99ba06b9514c1d10726`.

## Baseline acumulado

Corrida `tanda-48-sh46-abi-interop-r2-20260921`, finalizada sin fallos en 4030.470 s:

- 480/480 CTests con `lld-link` en 1986.552 s;
- 480/480 CTests con NSLink en 1898.992 s;
- 148/148 fixtures y 18/18 casos de triage por enlazador;
- `nsc.selfhost.interop-parity` aprobado en ambas matrices;
- instalación `A:\native-sharp` verificada con build `4800000000000002`;
- Gate 0 `LANDED`, cero errores por compilador, enlazador u optimización.

El primer intento de corrida no es evidencia de producto: falló antes de CTest al no poder reemplazar
temporalmente el ejecutable instalado y porque los árboles de pruebas aún no estaban estabilizados.
La corrida `r2` partió de la instalación y ambos árboles reconstruidos, y es el baseline normativo.

## Cierre

- [x] C y C++ reales;
- [x] callback y objeto con vtable estilo COM;
- [x] agregado de 8 bytes por parámetro y retorno;
- [x] promociones vararg;
- [x] vectorcall;
- [x] EXE y DLL con LLD y NSLink;
- [x] consumidor C de ambas DLL;
- [x] nueva autocompilación y punto fijo SH41;
- [x] baseline completo acumulado y Gate 0;
- [x] commit único de T48.

La integración en `development-claude` sigue dependiendo del cierre físico de T46/SH44; esta tanda
permanece en la pila aislada y no falsea esa atestación.
