# Plan de continuacion de NativeSharp: madurez nativa sin engordar el lenguaje

**Autor de este plan:** ChatGPT  
**Fecha:** 2026-09-03  
**Base:** Bloques 1 a 5 terminados; pipeline oficial verde con `lld-link` y NSLink.  
**Alcance:** lenguaje, compilador, runtime nativo minimo, biblioteca base y toolchain.  
**No es un plan de auto-hospedado.** No se va a reescribir `nsc` en NativeSharp durante estas
fases. El objetivo es que el lenguaje tenga la solidez que permitiria considerarlo en el futuro,
sin convertir esa posibilidad en trabajo actual.

Este documento **sustituye** cualquier lista preliminar de esta conversacion que presentara como
prioridad general caracteristicas de C# (`params`, `?.`, `Nullable<T>`, metodos de extension,
inicializadores, etc.) o bibliotecas de red/serializacion. Esas listas mezclaban inventario con
roadmap y no representan la direccion del producto.

---

## 1. Norte del producto

NativeSharp es un lenguaje nativo, pequeno y agradable para programacion ligera, principalmente
videojuegos y herramientas nativas relacionadas. Puede servir para escribir desde un juego pequeno
hasta un motor propio, pero **no es un motor** y no debe incorporar subsistemas de motor dentro del
lenguaje o de `NativeSharp.Core`.

La referencia de capacidad es lo que un programador espera de un toolchain C/C++ instalado de
fabrica:

- binarios nativos y estaticos cuando sea posible;
- memoria explicita, RAII y ownership verificable;
- punteros, layout, ABI C e interop de plataforma;
- genericos resueltos en compilacion;
- hilos, atomicos y reglas de memoria claras;
- archivos, colecciones, algoritmos y matematicas suficientes;
- builds Debug/Release, diagnosticos y depuracion nativa;
- enlace con bibliotecas nativas existentes;
- cero dependencia de un runtime administrado.

La referencia **no** es completar la superficie de C# ni competir con .NET.

---

## 2. Exclusiones permanentes

Estas piezas no entran en el plan. Encontrarlas en un inventario comparativo no las convierte en
trabajo pendiente.

- GC o tracing collector.
- CLR, IL, JIT o runtime administrado.
- Reflection en runtime, metadata dinamica o ejecucion dinamica de codigo.
- ASP.NET, servidores web, ORM, SQL o infraestructura enterprise.
- LINQ, expression trees y `dynamic`.
- `async`/`await` como subsistema administrado.
- Records, tipos anonimos y patrones complejos por paridad con C#.
- Un framework web, de serializacion general o de persistencia dentro de Core.
- ECS, scene graph, render graph, editor, asset pipeline u otros componentes de engine.
- Incorporar bibliotecas de terceros al runtime. Se consumen mediante bindings o enlace nativo.
- Self-hosting como entregable de este plan.

`comptime` o `#run` podria evaluarse en el futuro porque ejecutaria durante compilacion y produciria
un binario nativo estatico. No autoriza reflection ni codigo dinamico en el ejecutable final.

---

## 3. Regla para aceptar trabajo nuevo

Una caracteristica solo entra si supera las cuatro preguntas siguientes:

1. **Consumidor:** ¿existe codigo de juego, binding, biblioteca o toolchain que la necesite?
2. **Natividad:** ¿se puede resolver en compilacion o con una operacion nativa pequena?
3. **Peso:** ¿evita agregar un runtime, metadata o una abstraccion mayor que el problema?
4. **Prueba:** ¿se puede verificar en O0 y O2, y cuando corresponda con ambos enlazadores?

Si no existe consumidor medido, queda como candidato, no como gap obligatorio.

Cada cambio del compilador debe incluir:

- fixture positiva ejecutable;
- casos negativos con diagnostico concreto;
- comprobacion O0 y O2;
- `ctest` completo;
- corrida de fixtures con `lld-link` y NSLink;
- prueba ABI contra C o clang/MSVC cuando cambie layout o calling convention;
- manifiesto actualizado y verde.

No se abren dos cambios semanticos grandes a la vez. Una base roja no recibe features.

---

## 4. Base certificada

La continuacion parte de esta evidencia:

| Medicion | `lld-link` | NSLink |
|---|---:|---:|
| `ctest` | 244/244 | 244/244 |
| Fixtures | 96/96 | 96/96 |
| Triage historico | 16/16 | 16/16 |
| Manifiesto | 1302/1302 | 1302/1302 |

Los Bloques 1 a 5 ya dieron al lenguaje:

- ownership, `owned`, `borrowed`, `raii` y `move`;
- OOP, interfaces, virtuales, excepciones y cleanup;
- structs, unions, layout explicito, arrays fijos y ABI Win64;
- genericos monomorfizados, structs genericos y colecciones;
- `Span<T>`, `ReadOnlySpan<T>`, `StringView` y `Optional<T>`;
- parsing y formato numerico;
- IO de texto y binario, procesos y filesystem;
- `Algorithm`, `StringBuilder`, `TextWriter` y `ArenaAllocator`;
- emision de OBJ, EXE, LIB y DLL;
- dos enlazadores medidos sobre el mismo corpus.

No se vuelve a planificar ninguna de esas piezas.

---

## 5. Fase M1: correctness nativa y contratos

**Objetivo:** que ningun programa correcto dependa accidentalmente de LLVM, de una optimizacion o
de una convencion no escrita.

### M1.1 Cerrar los riesgos ya medidos

- Revalidar y, si sigue vivo, corregir ID 130: atributo `uwtable` con un valor que actualmente
  puede significar `None`. La prueba debe inspeccionar IR y ejecutar EH en Windows.
- Cerrar como verdadero o falso cada ID heredado que ya tenga una fixture equivalente. No crear
  una segunda fixture para el mismo comportamiento.
- Unificar el evaluador de constantes usado por campos `const`, etiquetas de `switch` y cualquier
  expresion que requiera evaluacion en compilacion.
- Adjuntar archivo, linea y columna a errores internos del lowering cuando exista una ubicacion de
  origen disponible.

### M1.2 Semantica de overflow

No se presupone que NativeSharp deba copiar `checked`/`unchecked` de C#.

Primero se decide y documenta:

- overflow de enteros con signo y sin signo;
- conversiones narrowing explicitas e implicitas;
- comportamiento en O0 y O2;
- si Debug inserta traps y Release usa wrap definido;
- folding de constantes bajo exactamente las mismas reglas.

Sólo si dos politicas dentro de un mismo programa tienen un consumidor real se agrega sintaxis
para seleccionarlas. La semantica debe existir antes que la palabra clave.

### M1.3 Modelo de memoria

Definir antes de implementar:

- que operaciones ordinarias pueden reordenarse;
- garantias de `AtomicInt32` y futuros atomicos;
- acquire, release, acquire-release y sequential consistency;
- fences explicitos;
- que significa acceso a memoria compartida sin atomicos;
- que significa `volatile` para MMIO o memoria observable.

`volatile` **no** sera una primitiva de sincronizacion entre hilos. La sincronizacion se expresa con
atomicos, fences, mutexes y monitores. Si se agrega `volatile`, su trabajo es impedir la eliminacion
o fusion de accesos observables, especialmente para dispositivos y memoria mapeada.

**Cierre de M1:** reglas escritas, IR comprobable, pruebas de ejecucion repetidas y cero diferencia
semantica entre O0 y O2 fuera de lo permitido por el contrato.

---

## 6. Fase M2: ABI y tipos para sistemas

**Objetivo:** declarar con precision estructuras y APIs nativas sin workarounds.

### M2.1 Enums con tipo subyacente

Implementar, como minimo:

```text
enum PixelFormat : byte
enum ResultCode : int
enum NativeFlags : uint
```

El tipo elegido debe gobernar `sizeof`, alineacion, campos, arrays, parametros, retornos y headers
C generados. Se certifica contra clang apuntando al ABI de MSVC.

### M2.2 Nombres de enum sin reflection

Evaluar ID 129 como utilidad de diagnostico. Si entra, el compilador genera una tabla estatica
nombre/valor únicamente para los enums que usen esa operacion. No hay descubrimiento de tipos,
metadata global ni reflection en runtime.

### M2.3 Unicode de plataforma

Congelar este contrato:

- `char` es una unidad de codigo de 8 bits;
- `string` almacena UTF-8;
- la indexacion trabaja sobre bytes/unidades de codigo, no promete puntos Unicode;
- conversion UTF-8/UTF-16 vive en una biblioteca nativa pequena para Win32;
- escapes `\u`/`\U` se resuelven durante lexing/compilacion.

No se incorpora cultura global, collation, normalizacion completa ni una base Unicode pesada en el
runtime. Funciones avanzadas pueden vivir en una biblioteca opcional.

### M2.4 Operaciones de bytes

Agregar sólo la superficie medida para interop y assets:

- lectura/escritura little-endian y big-endian;
- reinterpretacion segura entre escalares y spans de bytes;
- `Guid` de layout fijo para COM;
- conversion hexadecimal y Base64 sólo si aparece un consumidor real.

**Cierre de M2:** structs y enums cruzan C/NativeSharp en ambas direcciones sin discrepancias de
layout; UTF-8/UTF-16 hace round-trip con Win32; ninguna pieza requiere metadata dinamica.

---

## 7. Fase M3: genericos estaticos que no estorben

**Objetivo:** conservar la monomorfizacion y mejorar los puntos donde la implementacion actual
obliga a deformar una API.

### M3.1 Identidad de sobrecargas genericas

Corregir ID 132. La identidad no puede ser solamente nombre + aridad generica. Debe incluir la
forma de parametros necesaria para distinguir, por ejemplo:

```text
Sort<T>(Span<T>)
Sort<T>(Span<T>, Comparison<T>)
```

Se mantiene `SortWith` por compatibilidad durante una transicion; no se elimina sin una decision
explicita de compatibilidad.

### M3.2 Inferencia local de argumentos de tipo

Inferir `T` desde argumentos ordinarios cuando exista una solucion unica. No se implementa un
solver general ni inferencia global. Casos ambiguos deben pedir argumentos explicitos y producir un
diagnostico legible.

### M3.3 Restricciones e interfaces genericas, bajo demanda

No son prerrequisito para las colecciones actuales. Entran sólo cuando una API real necesite
expresar un contrato como comparacion, igualdad o enumeracion.

Si entran:

- son comprobaciones estaticas;
- no generan diccionarios de tipos ni reflection;
- no agregan dispatch dinamico salvo que el usuario ya haya pedido una interfaz virtual;
- cada instanciacion sigue monomorfizada.

**Cierre de M3:** desaparecen los nombres artificiales impuestos por el compilador, mejora el
diagnostico generico y el binario conserva el modelo estatico actual.

---

## 8. Fase M4: ergonomia pequena y de costo predecible

**Objetivo:** hacer bonito el codigo cotidiano sin agregar subsistemas.

### M4.1 Interpolacion de strings

Es la unica pieza de ergonomia prioritaria. Debe bajar en compilacion a `StringBuilder.Append` y a
las conversiones existentes. No requiere reflection, boxing ni un parser de formato en runtime.

### M4.2 Inicializacion estatica determinista

Diseñar constructores estaticos o una alternativa mas pequena con estas propiedades:

- una ejecucion por tipo;
- orden documentado;
- deteccion de ciclos;
- sin registro global de tipos;
- constantes puras resueltas en compilacion cuando sea posible.

### M4.3 Readonly por valor

Completar `readonly struct` o receptores `in` sólo donde evite copias medibles de tipos matematicos
o estructuras grandes. La regla debe ser de tipos y llamadas, no una optimizacion invisible.

### M4.4 Ausencias inventariadas que no son backlog

Los siguientes nombres pueden permanecer en documentos de auditoria para describir diferencias
con C#, pero **no forman parte de este plan ni cuentan como trabajo pendiente**:

- `params`, argumentos con nombre y parametros opcionales;
- `??`, `?.` y azucar de null;
- metodos de extension;
- inicializadores de objetos o colecciones;
- propiedades sólo `set`;
- lambdas anidadas;
- delegados a virtuales o receptores struct;
- `nameof`.

Tipos anidados sólo se reconsideran si una API nativa real no puede expresarse limpiamente sin
ellos. Ninguna ausencia de esta lista es criterio de madurez por si sola.

**Cierre de M4:** las mejoras reducen codigo repetitivo medido, tienen lowering directo y no agregan
servicios nuevos al runtime.

---

## 9. Fase M5: toolchain comparable con C++ nativo

**Objetivo:** que compilar, enlazar y depurar un juego no requiera mecanismos especiales del
repositorio.

### M5.1 Bibliotecas nativas externas

- Enlace declarado desde `.nsproj` con `.lib` y, donde el ABI sea compatible, `.a` de MinGW.
- Directorios de busqueda y orden de bibliotecas deterministas.
- Diagnostico claro de simbolo ausente, arquitectura incorrecta o formato incompatible.
- ABI C como frontera portable predeterminada; no prometer compatibilidad con name mangling o ABI
  de clases C++ entre MSVC, GCC y MinGW.
- Pruebas con una biblioteca producida por MSVC/clang-cl y otra por MinGW usando ABI C.

### M5.2 Configuraciones de build

Agregar un mecanismo pequeno de propiedades de proyecto para:

- Debug/Release;
- arquitectura y target;
- constantes de compilacion;
- nivel de optimizacion;
- checks de bounds, overflow y asserts según el contrato elegido.

No hace falta un preprocesador textual completo. Las propiedades del proyecto pueden inyectar
constantes conocidas por el compilador y dejar que la eliminacion de codigo muerto haga el resto.

### M5.3 Informacion de depuracion

Orden:

1. ubicaciones de origen completas en IR;
2. CodeView;
3. PDB utilizable por Visual Studio/WinDbg;
4. sidecar opcional de ownership para diagnosticar moves y lifetimes;
5. integracion DAP/LSP sólo despues de que la informacion base sea correcta.

### M5.4 Calidad de codigo y reproducibilidad

- builds reproducibles;
- politica determinista de punto flotante;
- sanitizers para programas NativeSharp cuando el backend los soporte;
- LTO/ThinLTO sólo despues de medir tamaño y tiempo de enlace;
- enlace incremental sólo si los tiempos reales lo justifican.

**Cierre de M5:** un proyecto fuera del repositorio puede configurar, compilar, enlazar una
biblioteca C externa, depurarse con simbolos y reproducir el mismo binario bajo la misma entrada.

---

## 10. Fase M6: interop de plataforma para videojuegos

**Objetivo:** acceder directamente a las APIs nativas de Windows y graficos sin convertir el
lenguaje en framework.

### M6.1 Fundacion COM estatica

Soportar lo necesario para D3D11/D3D12, WIC, WASAPI, DirectWrite y APIs semejantes:

- layout de vtable conocido en compilacion;
- llamada por slot con `this` explicito;
- `IUnknown`, `QueryInterface`, `AddRef` y `Release` expresables;
- `HRESULT` como entero/resultado nativo, sin excepciones automaticas obligatorias;
- `Guid`/IID/CLSID como valores de layout fijo;
- generacion o consumo de bindings sin reflection.

No se crea un runtime COM propio. NativeSharp sólo expresa la ABI existente.

### M6.2 Intrinsecos y SIMD

Agregar intrinsecos por familias pequeñas y medibles:

- operaciones escalares que ya mapea LLVM;
- vectores SIMD necesarios por la biblioteca matematica;
- carga, almacenamiento y alineacion explicitos;
- seleccion por target durante compilacion;
- fallback escalar donde sea razonable.

No se pone un motor matematico dentro de Core. La biblioteca de matematicas y los bindings siguen
siendo consumidores separados.

### M6.3 Red y formatos

No forman parte del lenguaje ni de Core.

Si un juego necesita red, audio comprimido, imagenes o formatos de assets, se resuelve mediante:

- bindings a bibliotecas nativas;
- paquetes opcionales y pequenos;
- implementaciones dentro del juego o motor del usuario.

No se agrega ASP.NET, HTTP de proposito general, SQL, ORM ni serializacion enterprise.

**Cierre de M6:** una aplicacion NativeSharp abre ventana, usa graficos/audio nativos y cruza COM o
ABI C con layout certificado, sin runtime administrado ni subsistemas de engine.

---

## 11. Fase M7: arquitectura interna preparada, sin auto-hospedado

**Objetivo:** reducir acoplamiento del compilador C++ y dejar fronteras limpias. Esto mejora la
madurez actual aunque nunca se escriba `nsc` en NativeSharp.

### M7.1 ABI estable del runtime nativo

- versionar nombres, firmas, ownership y errores de las operaciones runtime;
- separar la interfaz de su implementacion Win32;
- conservar dentro del compilador sólo el arranque y las piezas de EH que realmente necesiten
  sintesis;
- enlazar el resto como una biblioteca nativa versionada.

### M7.2 Frontera del backend

- congelar el subconjunto usado de la API C de LLVM;
- eliminar llamadas LLVM dispersas fuera del backend;
- expresar en MIR las decisiones semanticas antes de llegar a LLVM;
- permitir que otro backend sea posible sin comprometerse a construirlo.

### M7.3 Lo que esta fase no hace

- no crea `nsc-next`;
- no traduce archivos C++ a NativeSharp;
- no inicia `nsc0 -> nsc1`;
- no obliga a escribir NSASM;
- no convierte LLVM y self-hosting en el mismo proyecto.

**Cierre de M7:** runtime y backend tienen contratos versionados, el compilador sigue siendo C++ y
la suite no cambia de comportamiento.

---

## 12. Orden de ejecucion recomendado

```text
Base verde
   |
   +--> M1  Correctness y contratos
   |      overflow + memoria + diagnosticos + deuda medida
   |
   +--> M2  ABI y tipos de sistemas
   |      enum base + UTF-8/UTF-16 + bytes + Guid
   |
   +--> M3  Genericos estaticos
   |      identidad + inferencia; constraints sólo con consumidor
   |
   +--> M4  Ergonomia pequeña
   |      interpolacion + inicializacion estatica + readonly medido
   |
   +--> M5  Toolchain nativo
   |      bibliotecas externas + perfiles + PDB + reproducibilidad
   |
   +--> M6  Interop de videojuegos
   |      COM estatico + SIMD/intrinsecos
   |
   `--> M7  Fronteras internas
          runtime ABI + backend, sin self-hosting
```

M1 es obligatorio primero. M2 y M5 son los que mas acercan NativeSharp a la experiencia de un
toolchain C++ utilizable. M3 y M4 hacen el lenguaje mas agradable sin hacerlo pesado. M6 se abre
cuando exista un consumidor grafico/audio concreto. M7 puede adelantarse sólo para resolver deuda
que este bloqueando una fase anterior.

---

## 13. Primera ronda concreta

La siguiente ronda de implementacion debe ser pequeña y cerrar una sola propiedad:

### Ronda M1-A: contrato y auditoria de semantica nativa

1. Confirmar el estado real del ID 130 (`uwtable`) en el árbol actual.
2. Escribir la decision de overflow de NativeSharp sin agregar todavía sintaxis.
3. Escribir el contrato de memoria/atomicos/`volatile` sin agregar todavía la keyword.
4. Añadir fixtures que demuestren el comportamiento actual y detecten divergencias O0/O2.
5. Clasificar cada diferencia encontrada como bug, decision pendiente o comportamiento permitido.
6. Implementar únicamente los bugs demostrados por esas fixtures.
7. Cerrar con las dos pasadas oficiales verdes.

**Entregables:** documento de contrato, fixtures, fixes demostrados y evidencia.  
**No entregables:** nuevas APIs de red, self-hosting, GC, reflection, engine o features de C# por
paridad.

---

## 14. Definicion de madurez buscada

NativeSharp se considera suficientemente maduro para su objetivo cuando:

- un juego o herramienta nativa puede vivir completamente sin runtime administrado;
- O0 y O2 respetan la misma semantica observable;
- ownership y ABI fallan con diagnosticos, no con corrupcion silenciosa;
- las APIs C y COM de Windows se expresan sin thunks manuales especiales;
- las bibliotecas externas se enlazan desde el proyecto;
- el programa se depura con archivo, linea y simbolos nativos;
- concurrencia y atomicos tienen un contrato real;
- UTF-8/UTF-16 cruza Win32 sin conversiones improvisadas;
- el lenguaje sigue siendo pequeño y el runtime sigue siendo prescindible o estatico;
- cada feature existente tiene un consumidor y una prueba.

La posibilidad futura de auto-hospedado sera una consecuencia de estas capacidades, no el motivo
para inflar el lenguaje ni una tarea implicita de este plan.
