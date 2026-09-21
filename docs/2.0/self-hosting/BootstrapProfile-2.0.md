# NativeSharp Bootstrap Profile 2.0

**Contrato:** `NativeSharp.BootstrapProfile/2.0`

**Formato ejecutable:** `selfhost/bootstrap/bootstrap-profile.json`, versión 1

**Host y target inicial:** Windows x64, ABI MSVC/COFF/PE

**Compilador semilla:** `nsc0`, NativeSharp 2.0 Stage 70

**Estado:** congelado por Gate SH01

## 1. Propósito

Este perfil es el subconjunto de NativeSharp 2.0 autorizado para escribir `nsc-next`. No define un
lenguaje nuevo ni reduce lo que NativeSharp 2.0 ofrece a sus usuarios. Su función es impedir una
dependencia circular: toda construcción usada por `nsc-next` debe poder compilarse con el `nsc0`
bloqueado por SH00, y ninguna comodidad del compilador nuevo puede ser necesaria para construir la
primera generación.

La autoridad semántica continúa siendo la especificación NativeSharp 2.0. Este documento decide
qué parte de esa especificación puede usar el compilador autoalojado. El JSON ejecutable contiene
la lista exacta de palabras reservadas, restricciones, diagnósticos y proyectos de biblioteca.

## 2. Invariantes

1. `nsc0` compila todas las construcciones positivas del perfil.
2. El verificador corre antes de cualquier etapa `nsc1`, `nsc2` o `nsc3`.
3. Una infracción termina con código 2 y no produce artefactos de bootstrap.
4. Un contrato ilegible, desconocido o una entrada inválida termina con código 3.
5. Comentarios y literales no cuentan como uso de una construcción.
6. El orden de archivos y diagnósticos es determinista y no depende del filesystem.
7. El perfil permite corrección completa del compilador; no autoriza optimizaciones específicas de
   plataforma ni paralelismo antes de sus gates.
8. Ampliar el perfil exige modificar contrato, fixtures, documentación y baseline en el mismo
   commit. Una ampliación nunca se infiere del código existente.

## 3. Inventario de construcciones autorizadas

### 3.1 Unidades, nombres y declaraciones

- archivos UTF-8, `using`, namespaces y nombres calificados;
- clases, clases anidadas, structs, readonly structs, borrowed structs, interfaces y enums;
- aliases y tipos anidados cuando su identidad sea explícita;
- accesibilidad `public`, `internal`, `protected` y `private`;
- miembros de instancia y estáticos, constantes, campos readonly, propiedades e indexers;
- constructores, destructores automáticos, métodos virtuales, abstractos, sellados y overrides;
- operadores y conversiones explícitas o implícitas cuando una abstracción de Basic lo requiera;
- declaraciones parciales sólo para fronteras generadas cuyo orden canónico tenga prueba.

No se permite depender de orden físico de declaración, dirección de memoria o enumeración del
filesystem para crear una identidad semántica.

### 3.2 Tipos y expresiones

- todos los tipos primitivos de NativeSharp 2.0;
- `string`, arrays unidimensionales, punteros crudos, referencias `ref`/`in`/`out` y null;
- casts explícitos, conversiones numéricas definidas por 2.0, `is`, `as`, `sizeof`, `alignof` y
  `offsetof`;
- acceso a miembro, llamada, indexación, operadores aritméticos, lógicos, bit a bit, comparación,
  asignación simple/compuesta, incremento/decremento y condicional `?:`;
- `new` ordinario, `delete` explícito cuando el dueño está visible y `default(T)`;
- inicializadores de arrays y agregados únicamente si producen el mismo modelo semántico en `nsc0`.

Overflow y promociones obedecen la especificación 2.0. El compilador no puede usar overflow nativo
accidental como parte de un hash, ID o recorrido canónico.

### 3.3 Control de flujo y errores

- `if`/`else`, `while`, `do`, `for`, `foreach`, `switch`, `break`, `continue` y `return`;
- `try`, `catch`, `finally` y `throw` en fronteras que puedan recuperar o añadir contexto;
- excepciones NativeSharp para fallos excepcionales; resultados/diagnósticos explícitos para error
  normal de usuario;
- `using` de recursos sólo cuando el destructor y el orden de cleanup estén cubiertos por tests.

Se prohíbe esconder una excepción a través de una frontera C. Toda callback C captura la excepción
y la convierte al resultado explícito definido por esa frontera.

### 3.4 Genéricos

- clases, structs, interfaces, métodos y delegates genéricos;
- constraints `class`, `struct`, interface y base; inferencia ya certificada por `nsc0`;
- monomorfización AOT y referencias cruzadas entre los cuatro proyectos de stdlib permitidos;
- specialization únicamente por tipo y contrato, nunca por dirección, hora u orden de workers.

Quedan fuera los constraints no implementados por la semántica 2.0 bloqueada y cualquier esquema
que requiera reflexión o generación de código durante el bootstrap.

### 3.5 Ownership, préstamos y vida

- `owned`, `borrowed`, `raii`, `move` y vistas borrowed;
- clases dueñas, valores sin ownership, arrays propietarios y arenas con destrucción determinista;
- transferencia explícita en parámetros, retornos y colecciones;
- préstamos que no escapan y handles de arena cuya validez se comprueba por generación;
- cleanup automático en retornos, excepciones y salidas de scope.

Para el compilador se prefieren arenas por fase y valores inmutables. `stackalloc` queda permitido
para arreglos temporales acotados cuyo puntero no escapa de la llamada (por ejemplo, vectores de
argumentos de LLVM-C); SH23 y SH27 certifican su lowering. `destroy`, placement, VLA y layout manual
permanecen fuera.

### 3.6 Punteros, delegates y código inseguro

- punteros a datos para buffers y objetos opacos de LLVM C;
- `extern C delegate` para las firmas exactas del binding LLVM;
- delegates administrados y lambdas sólo donde no capturen owners ni cambien el orden observable;
- casts de puntero explícitos exclusivamente dentro de `bindings/` o una frontera marcada;
- toda firma externa declara calling convention, nullability práctica y ownership en su contrato.

No se autorizan `vectorcall`, COM, thunks COM, eventos, ensamblador integrado, uniones, layout
explícito ni intrinsics de hardware. La única calling convention externa del bootstrap inicial es
C sobre Win64.

## 4. Subconjunto inicial de biblioteca

El verificador permite únicamente estos proyectos; el nombre del `.nsproj` forma parte del
contrato ejecutable:

| Proyecto | Superficie autorizada |
|---|---|
| `NativeSharp.Core` | Object/Exception, String/StringView/StringBuilder/Utf8, Span/ReadOnlySpan, Optional, ByteBuffer, BinaryReader/Writer, ArenaAllocator, Environment, Console y primitivas numéricas deterministas |
| `NativeSharp.Core.Collections.Generic` | List, Dictionary, HashSet, Stack, Queue, KeyValuePair, Algorithm, hashing/comparación y enumeradores requeridos |
| `NativeSharp.Core.IO` | Path, File, Directory y TextWriter |
| `NativeSharp.Core.Diagnostics` | Process, Stopwatch, SystemInfo y Debug |

`NativeSharp.Core.Threading` está prohibido hasta que el determinismo con múltiples workers tenga
un gate propio. Random, reloj de pared y locale no pueden alimentar salidas reproducibles. Los
tipos SIMD y hardware intrinsics tampoco pertenecen al perfil aunque vivan físicamente en Core.

Las únicas fronteras externas previstas son `NativeSharp.LLVM.C` y
`NativeSharp.Win32.Compiler`. SH03 deberá crearlas y congelarlas antes de que el frontend las use.

## 5. Construcciones rechazadas y razón

Los códigos `NSBPxxxx` son estables:

| Familia | Rechazo |
|---|---|
| `NSBP2001–2009` | volatile, política forzada de inline, destroy, exports, vectorcall, events y comptime (`NSBP2005` queda retirado) |
| `NSBP2101–2108` | assembly integrado, uniones, placement y layout manual (`pack`, `at`, `size`, `align`) |
| `NSBP2201–2203` | threading e intrinsics de hardware |
| `NSBP3001` | ProjectReference fuera de los cuatro proyectos permitidos |
| `NSBP3002` | PackageReference o NativeReference externa |
| `NSBP1099` | palabra reservada reconocida por 2.0 pero no aprobada por este perfil |

Estas exclusiones no califican las características como defectuosas. Reducen el bootstrap a una
superficie que ya tiene evidencia y evita fuentes de circularidad o no determinismo.

## 6. Verificación ejecutable

Comando canónico:

```text
python -B selfhost/bootstrap/verify_profile.py ^
  --profile selfhost/bootstrap/bootstrap-profile.json ^
  --source-root selfhost/nsc
```

Opciones:

- `--source-root` se puede repetir; acepta archivo o directorio;
- `--profile` selecciona explícitamente el contrato v1;
- `--report` escribe el reporte JSON versionado;
- `--quiet` suprime la salida humana, sin cambiar códigos de salida.

El verificador tokeniza de forma determinista, elimina comentarios y literales, comprueba palabras
reservadas y secuencias prohibidas, inspecciona imports calificados y analiza referencias de
proyecto XML. No sustituye al parser ni a Sema: después del filtro, `nsc0` sigue siendo quien valida
la sintaxis y semántica completa. La fixture positiva se filtra, compila con `nsc0` en O0 y debe
devolver 42. Cada fixture negativa fija al menos un código `NSBP`.

## 7. Evolución

El perfil 2.0 queda congelado al aterrizar SH01. Una necesidad nueva se resuelve en este orden:

1. demostrar que no existe una forma equivalente dentro del perfil;
2. comprobar que `nsc0` y la especificación 2.0 ya soportan la construcción;
3. añadir regla, inventario y fixtures positivas/negativas;
4. ejecutar el baseline completo;
5. registrar la decisión en el expediente de la tanda que solicita la ampliación.

Una versión futura del formato no se acepta silenciosamente: el lector v1 falla con código 3.
