# VertexBuffer.SetData<T> y GetData<T> desde un consumidor separado

Respuesta a `INVESTIGACION-VERTEXBUFFER-SETDATA-GETDATA.md`. La causa de cada fallo es distinta. Las dos se
reprodujeron con bibliotecas mínimas propias, sin SaruDX ni D3D11. **SaruDX no se validó aquí.**

Base: `b9bfbc1c9` en `development_cpp__clang_integracion`.

| Problema | Commit | Fixture |
|----------|--------|---------|
| 1. `GetData<T>`: símbolo no publicado | `6b5af9fe3` | `pruebas-interop-plantilla-campo-instancia` |
| 2. `SetData<T>`: `0xC0000005` | `eee5d18d5` | `pruebas-interop-genericos-virtuales-importados` |

## 1. GetData<T>: símbolo interno rechazado como no publicado

**Reproducción.** El productor estático tiene `Buffer.GetData<T>`, que llama `store.Read(...)`, donde `store` es un
campo privado de instancia de tipo `Store` (interno). El consumidor dio el mismo error del informe:
`unpublished symbol '__nsc_method_VbLib$3A$3AProd$2EStore__Read__...'`. El símbolo está definido en la `.lib`.

**Causa (demostrada).** En `validateLibraryReferenceSymbols`, la ruta provisional para helpers de plantillas
publica los métodos de los tipos de los campos del dueño de la plantilla, pero solo de los **estáticos**. Al volcar
los `metadata.members` del caso, el campo `store` aparece con `flags=0` (de instancia) y
`declaredTypeKey=type:VbLib::Prod.Store`; `Store.Read` es público y está en el archivo. No había otra vía que lo
cubriera: `candidateReceiverTypeKeys` solo registra receptores escritos como nombre de tipo, y el texto de la plantilla
dice `store.Read`, no `Store`. La hipótesis del informe era correcta.

**Corrección.** También se recogen los tipos de los campos de instancia del dueño. Las demás comprobaciones no
cambian:

- el símbolo debe estar en el archivo;
- debe ser del mismo módulo, sin duplicados entre módulos;
- se excluyen `extern C` e importados C;
- la ruta solo aplica a bibliotecas estáticas (`runtimePath` vacío).

**Controles.**

- Si `Store.Read` se renombra dentro de la `.lib` con `llvm-objcopy --redefine-sym`, el consumidor se sigue
  rechazando.
- Sin la corrección, el fixture falla en `O0-consumidor` con el error original.

**Límite (DLL).** Con el productor como `SharedLibrary`, la DLL solo exporta los miembros públicos de `Buffer`.
`Store.Read` no se exporta y el consumidor se sigue rechazando: no podría enlazarlo. Admitirlo requeriría que el
productor exporte los helpers de sus plantillas, un cambio de productor que no se hizo. Si SaruDX consume
`SaruDX11` como DLL, `GetData<T>` seguirá fallando; con `.lib` estática, no.

## 2. SetData<T>: violación de acceso en la llamada virtual genérica

**Reproducción.** El productor estático tiene:

- `SetData<T>(T[])`, no virtual;
- `virtual SetData<T>(T[], int, int)`, que delega en `virtual SetData<T>(int, T[], int, int, int)`.

El consumidor imprime `101` y muere en `buffer.SetData<int>(null, 0, 0)` (Segmentation fault). En un solo proyecto,
el mismo código devuelve el valor correcto.

**Causa (demostrada con el IR).** Un método genérico virtual no tiene slots en la plantilla; cada especialización
recibe uno al construirse el modelo de despacho:

- **Productor:** no instancia `SetData<int>` y emite `__nsc_vtable_VbLib::Prod.Buffer` con 3 entradas: `ToString`,
  `Equals` y `GetHashCode`.
- **Consumidor:** materializa `SetData<int>`. Su modelo (`--dump-project-virtual-dispatch`) da a la clase importada
  5 slots: `slot[3]` para la especialización de 3 argumentos y `slot[4]` para la de 5.
- **IR del consumidor:** carga `getelementptr ptr, ptr %vtable, i64 3` (y `i64 4` dentro de la especialización)
  sobre la vtable del productor, que tiene 3 entradas.

La lectura cae fuera de la tabla. Esto explica todas las observaciones del informe:

- Falla con `null` y con longitud cero, porque el despacho ocurre antes de entrar al cuerpo.
- `SetData<T>(null)` de un argumento retorna bien, porque no es virtual y sale antes de delegar.
- Pasa igual con la biblioteca base y con la migrada.

El `callq *0x30(%rax)` que observaron corresponde al slot 6 (`0x30 / 8`). Encaja con una clase que hereda más
slots de `GraphicsResource` y añade los de las especializaciones, pero aquí no puedo comprobarlo sin SaruDX. El
resultado depende de lo que haya en memoria tras la tabla. En el fixture, sin la corrección, el
consumidor O0 no se cae: devuelve un valor erróneo (`1`) porque salta a otra función.

**Corrección.** Una especialización virtual (no abstracta) de un método genérico declarado en una clase
precompilada no recibe slot en el modelo de despacho del consumidor. MIR la emite como llamada directa, y también como
valor de método (delegado).

Es el mismo destino que el slot tiene en un solo proyecto. Ahí ningún override de una especialización genérica llega a
registrarse: el nombre de la especialización lleva la clave de su propia plantilla. Si una subclase redefine
`SetData<T>`, su especialización da "override method ... has no inherited method to override", o no se instancia y el
slot queda con la especialización base. Se comprobó con una subclase `Dyn` en un proyecto y entre módulos.

**Lo que no cambia:**

- las vtables y los metadatos del productor;
- las clases locales;
- los virtuales no genéricos.

**Controles en el fixture (O0–O3, mismo proyecto, `.lib` y DLL):**

- 1, 3 y 5 argumentos con datos, `null` y longitud cero;
- `GetData<T>` virtual;
- una sobrecarga genérica no virtual del mismo nombre;
- virtual no genérico redefinido en una subclase del productor y en una subclase local del consumidor, por referencia
  base;
- `SetData<T>` sobre ambas subclases.

## Defecto aparte, no corregido: overrides de métodos genéricos virtuales

Hay que distinguirlo del crash. Hoy, dentro de un proyecto y entre módulos, `override` de un método genérico virtual
no participa en el despacho:

- **Por referencia base:** se ejecuta la especialización de la base.
- **Llamada directa sobre la subclase:** no compila, con
  `receiver/type '...Buffer' has no compatible member named 'SetData__g1__int__h...'` si el override llama
  `base.SetData<T>`, o "has no inherited method to override" si no.

Corregirlo requiere instanciar los overrides de cada especialización base y un despacho que no dependa de slots fijos
entre módulos; es un diseño nuevo.

**Qué revisar en SaruDX:** si `DynamicVertexBuffer` u otra clase redefine un `SetData<T>`/`GetData<T>` virtual,
ese override no se ejecuta por referencia base, ni antes ni después de este cambio.

## Comandos

```
cmake --build build-integracion --target nsc
set NSC_LINKER=A:\nslink-devel\bin\release\nslink.exe
python nativesharp-interop\pruebas-interop-plantilla-campo-instancia\validar.py --nsc <nsc.exe> ^
    --core-root <build>\nativesharp-libraries-build --nslink <nslink.exe> [--llvm-objcopy <llvm-objcopy.exe>]
python nativesharp-interop\pruebas-interop-genericos-virtuales-importados\validar.py --nsc <nsc.exe> ^
    --core-root <build>\nativesharp-libraries-build --nslink <nslink.exe>
```

## Resultados

| Prueba | O0 | O1 | O2 | O3 |
|--------|----|----|----|----|
| `pruebas-interop-plantilla-campo-instancia` (mismo proyecto y consumidor `.lib`) | ✓ | ✓ | ✓ | ✓ |
| `pruebas-interop-genericos-virtuales-importados` (mismo proyecto, `.lib` y DLL) | ✓ | ✓ | ✓ | ✓ |
| `pruebas-interop-genericos-receptor` (regresión de `643ed45bb`) | ✓ | ✓ | ✓ | ✓ |

- **Controles en O0:** símbolo renombrado rechazado; DLL con helper interno rechazada (límite).
- **Sin cada corrección:** su fixture falla (`O0-consumidor` y `O0-estatica-run`).
- **ctest dirigidas, con `nslink.exe` en `PATH`:** pasan.
  - `lowering-and-dispatch-boundaries`
  - `sealed-override-dispatch` y `sealed-override-dispatch-binary`
  - `stage34.virtual-*` y `stage38.interface-dispatch`
  - `stage44.*`
  - `generics.*binary*` y `generics.owner-method*`
  - `generic-delegate-return-binary`
  - `fixture.integration-libraries`
  - `visibility.internal-static-type`
- **Omitida por entorno:** `shared-receiver-dispatch-regression`.

Metadatos: el formato no cambia; no hay que regenerar bibliotecas. Hay que recompilar los **consumidores** con el NSC
nuevo; las bibliotecas no, aunque recompilarlas no hace daño.

## Qué repetir en SaruDX

1. Compilar los consumidores de `vertexget-*` y `vertexset-*` en O0–O3 contra la base y la migrada, con `SaruDX11`
   como `.lib` estática. Deben imprimir `106` y devolver `0`; `GetData` debe devolver los valores de entrada.
2. Si `SaruDX11` se consume como DLL, `GetData<T>` seguirá con "unpublished symbol" (límite de DLL).
3. Revisar si `DynamicVertexBuffer` u otra subclase redefine un `SetData<T>`/`GetData<T>` virtual (defecto aparte).
