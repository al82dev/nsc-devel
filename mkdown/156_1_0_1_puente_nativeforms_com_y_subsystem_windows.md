# 156 — NativeSharp 1.0.1: puente NativeForms/COM y `/subsystem:windows`

**Fecha:** 2026-09-08

**Línea de trabajo:** 1.0.1, sobre Stage 67

**Origen del reporte:** `A:\SaruCore\NativeForms\mkdown\NF_COMPILER_REQUESTS_COM.md`

## 1. Alcance y decisión

El reporte separa correctamente una necesidad de biblioteca de una necesidad del compilador. No
se agregó un sistema de tipos COM, `[ComImport]`, generación de vtables, apartments, IDL ni
marshalling. NativeForms conserva la responsabilidad de escribir la capa de vtables.

El resultado de cada petición es:

| petición | resultado |
|---|---|
| R1, `void*` → `extern C delegate` | ya implementada y ya cubierta por conformidad |
| R2, `extern C delegate` → `void*` | ya implementada y ya cubierta por conformidad |
| R3, literal GUID | diferida; es ergonomía y no bloquea COM |
| R4, `/subsystem:windows` | implementada en esta ronda |
| R5, lambda frente a overload delegate/no-delegate | implementada en esta ronda |

## 2. R1 y R2: no había que reimplementarlas

`ProjectMethodBodyModel` ya clasifica explícitamente las conversiones en ambas direcciones entre
un `extern C delegate`, `nint`/`nuint` y un puntero crudo, incluido `void*`. También permite la
reinterpretación explícita entre dos firmas C y mantiene fuera de esa regla al delegate ordinario,
que es una celda code/context y no una dirección.

La evidencia permanente ya existía:

- `tests/interop_contract/c1_05_raw_pointer_cast/Main.ns` hace function pointer → `nint` → function
  pointer, function pointer → `void*` → function pointer y reinterpretación entre firmas C;
- `tests/unit/Stage56C1InteropCompletionTests.cpp` protege los positivos y los rechazos de delegates
  ordinarios;
- `spec/NativeSharp_1_0_conformance.map` ya liga esa evidencia con `NS-PTR-001`.

El caso `c1_05_raw_pointer_cast` se compiló y ejecutó con LLD y NSLink, en O0 y O2. Las cuatro
combinaciones compilaron con salida 0 y el programa devolvió 42.

## 3. ComProbe de NativeForms

El `build_comprobe.bat` original no alcanzó el compilador de COM. Su `.nsproj` escribe
`<OutputType>Executable</OutputType>`, mientras que el contrato vigente admite `Exe`, `Library`,
`StaticLibrary` y `SharedLibrary`; por eso el script termina antes de compilar `Main.ns`.

Para medir el contenido sin modificar el repositorio consumidor se hizo una copia del probe y se
cambió únicamente ese valor a `Exe`. Con el compilador instalado
`0.9.0+build.20260908T172933Z.2ddf0cc88b7b129a`, las siete comprobaciones imprimieron `PASS` y el
programa devolvió 0 en los cuatro cuadrantes:

| linker | O0 | O2 |
|---|---:|---:|
| LLD | 0 | 0 |
| NSLink | 0 | 0 |

Eso incluye la forma completa `void*` → vtable → slot → cast a `extern C delegate` → llamada. El
carril COM queda cerrado: es trabajo de biblioteca, no una nueva feature del compilador.

## 4. R3: GUID queda fuera del camino crítico

Un literal GUID reduciría errores al escribir los once componentes del layout mixto, pero no
agrega una capacidad necesaria. NativeForms puede generar hoy un struct compuesto por `uint`, dos
`ushort` y ocho `byte`. Esta ronda no introduce sintaxis, atributos ni operaciones especiales para
GUID y R3 no debe retrasar 1.0.1.

## 5. R4: selección explícita del subsistema PE

Antes, `LldLinker` agregaba siempre `/subsystem:console` y `PeExecutableVerifier` rechazaba
correctamente cualquier imagen que no fuera CUI. El driver no tenía forma de expresar la elección.

Ahora las tres formas que emiten ejecutables aceptan, en cualquier posición y sin distinguir
mayúsculas:

```text
/subsystem:console
/subsystem:windows
```

La omisión conserva `console`; no hay cambio silencioso para proyectos existentes. `windows`
produce `IMAGE_SUBSYSTEM_WINDOWS_GUI` (valor 2), evita que Windows cree la consola y no modifica el
entry point de NativeSharp. La selección atraviesa `Driver`, `WindowsExecutableBuilder` y
`LldLinker`, por lo que funciona igual con `lld-link.exe` y `nslink.exe`.

Un valor desconocido, una selección duplicada o el uso del flag en una acción que no emite un
ejecutable devuelve error de línea de comandos. El verificador PE admite GUI y CUI, pero continúa
rechazando cualquier otro valor.

La cláusula normativa `NSC-LINK-001` y su fila de conformidad fijan este comportamiento.

## 6. R5: una lambda descarta overloads que no son delegate

NativeForms necesitaba poder conservar un nombre como `BeginInvoke` para dos formas:

```csharp
bool BeginInvoke(InvokeAction work);
bool BeginInvoke(Runnable work);
```

La expresión `BeginInvoke(() => { ... })` sólo puede convertirse al primer parámetro. Sin embargo,
el binder anterior conservaba el overload de `Runnable` durante la búsqueda de contexto y dejaba a
la lambda sin tipo destino.

El filtro ahora reúne únicamente los tipos delegate compatibles con la cantidad de parámetros y
con cualquier tipo explícito escrito en la lambda. Si queda un solo tipo delegate, ése proporciona
el contexto aunque haya overloads no-delegate. Si quedan dos tipos delegate distintos, la lambda
sin tipos sigue siendo rechazada: no se adivina una firma mediante el cuerpo.

El caso rojo `AmbiguousCallTarget` en `Stage43LambdasClosuresTests.cpp` conserva ese límite. La
fixture ejecutable de Stage 67 agregó el positivo delegate/no-delegate y lo ejecuta en O0/O2. La
redacción de `NS-DELEGATE-001` se apretó para expresar la regla completa, no sólo el caso de dos
delegates.

## 7. Archivos modificados

- `include/nativesharp/Linker/LldLinker.h`
- `src/Linker/LldLinker.cpp`
- `include/nativesharp/Linker/WindowsExecutableBuilder.h`
- `src/Linker/WindowsExecutableBuilder.cpp`
- `src/Linker/PeExecutableVerifier.cpp`
- `include/nativesharp/Driver/Driver.h`
- `src/Driver/Driver.cpp`
- `tests/unit/WindowsLinkerTests.cpp`
- `tests/unit/DriverTests.cpp`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `tests/unit/Stage67E11LambdaExecutionTests.cpp`
- `spec/NativeSharp_Language_Specification_1_0.md`
- `spec/NativeSharp_Compiler_Contract_1_0.md`
- `spec/NativeSharp_Compiler_1_0_conformance.map`

## 8. Evidencia previa al baseline

El build incremental final de `A:\nsc-devel\build-clang` terminó correctamente con identidad
`0.9.0+build.20260908T201609Z.f6d8db68c6fe3d08`.

Una compilación real de `full_frontend.ns` con `/subsystem:windows` produjo con cada linker una
imagen PE cuyo campo subsystem es 2. Ambos programas se ejecutaron y devolvieron 43.

Las pruebas focalizadas se construyeron y ejecutaron en las dos configuraciones:

```text
build-test-lldlinker: nsc.windows-linker PASS, nsc.driver PASS
build-test-nslink:    nsc.windows-linker PASS, nsc.driver PASS
```

Para R5, `nsc.stage43.lambdas-closures` y `nsc.stage67.e11-lambda-execution` pasaron también en
ambas configuraciones. La segunda construyó y ejecutó el positivo a O0 y O2, mientras la primera
conservó el rechazo de dos destinos delegate distintos.

No se ejecutó G00. La promoción del baseline permanece en manos del usuario después de instalar el
build rápido y validar el consumidor real.
