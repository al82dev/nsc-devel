# 126 — Native Sharp 0.8.0-alpha: cierre de los workarounds del juego

**Fecha:** 2026-09-06  
**Versión:** `nsc 0.8.0-alpha`  
**Baseline instalado anterior:** `0.8.0-alpha+build.20260906T081445Z.b9aa99a37c342041`

**Árbol actual:** matriz local 297/297 verde; instalación y G00 de la nueva identidad pendientes.
**Dominio:** lenguaje y compilador nativo para juegos, gráficos y plataforma.

## Decisión de versión

Esta entrega merece el salto a `0.8.0-alpha`: no es una suma de APIs, sino una mejora coherente
de semántica del lenguaje bajo un consumidor grande. El juego ya compilaba con expansiones y
ownership manual; 0.8 incorpora al compilador las formas seguras y generales descubiertas por ese
port. La identidad alpha anterior está certificada; las correcciones posteriores de identidad
genérica y compatibilidad C# útil movieron el árbol. Promover el texto de versión a `0.8.0`
requiere instalar y certificar esta identidad exacta.

No forman parte del objetivo web, SQL, LINQ, bases de datos ni aplicaciones empresariales. Añadir
esas superficies diluiría el lenguaje y no cerraría ningún problema de código nativo, FPS,
latencia, memoria o interoperabilidad de plataforma.

## Lo que cierra 0.8.0-alpha

### Lifetime de temporales string — NSC-LIMIT-021

Un resultado `owned string` fresco puede prestarse a un parámetro `borrowed string` durante la
expresión completa. HIR/MIR representan el owner temporal y su cleanup; LLVM mantiene el cleanup
activo si fallan argumentos posteriores o la llamada. APIs de usuario sin contrato `borrowed`
siguen sin poder retener accidentalmente el temporal.

### Arrays nativos — NSC-LIMIT-016 y NSC-LIMIT-017

`raii T[]` es ahora un owner del buffer: se libera al salir, participa en unwind y se puede mover
sin doble liberación. La propiedad de los elementos no se vuelve implícita; NativeSharp conserva
la semántica explícita y de bajo nivel de sus arrays.

`new T[] { ... }` infiere el tamaño, admite coma final, evalúa cada elemento una sola vez y de
izquierda a derecha, aplica sus conversiones y conserva `Length`. Esto reemplaza expansiones de
tablas grandes por índices sin convertir el lenguaje en un runtime de colecciones administradas.

### Especialización genérica por demanda — NSC-LIMIT-018

Una especialización expone todas sus firmas para lookup, pero sólo instancia cuerpos ordinarios
seleccionados por una llamada o delegate. Constructores, destructores y virtuales siguen eager.
Por ello `List<Pair>` puede usar almacenamiento, `Add` e indexador aunque `Pair` no tenga `<`;
si el programa llama a un miembro que realmente ordena, el cuerpo se valida y el error reaparece.

### Parámetros predeterminados — NSC-LIMIT-020

Métodos y constructores aceptan valores literales predeterminados en el grafo de proyectos fuente.
La resolución admite omitir parámetros finales, prefiere la sobrecarga que usa menos defaults y
HIR materializa cada valor con el tipo del parámetro. No se afirma todavía compatibilidad de
defaults serializados dentro de metadata `.lib` precompilada.

## Exclusión deliberada: `static owned`

0.8 no permite `static owned`. Permitir la sintaxis sin una fase real de finalización de módulo
produciría fugas o un contrato falso de destrucción. Para estado estático, el patrón soportado
sigue siendo referencia no propietaria, transferencia explícita y liberación manual controlada.
Esto no es una concesión a C#: C# puede apoyar ese lifetime en GC; NativeSharp no tiene GC y no
debe fingirlo.

## Pruebas de cierre

- `c49`: préstamo temporal string, O0/O2 y resultado 42.
- `c50`: inicializador inferido, orden, structs, `Length`, `raii T[]` y `move`, O0/O2.
- `c51`: default omitido/explícito, múltiples opcionales, overload y constructor, O0/O2.
- `c52`: `List<Pair>` sin operadores de orden usa sólo miembros alcanzados, O0/O2.
- Caso negativo: un miembro genérico no válido sigue diagnosticándose cuando sí es invocado.

La certificación de release exige reconstrucción limpia, suite completa y G00 instalado sobre el
hash exacto de la entrega; el baseline no se hereda de una identidad anterior.

La primera matriz CTest terminó **297/297 verde**, pero ese verde no ejecutaba en Windows las
fixtures `c25`–`c28`: el arnés interno sólo comprobaba que compilaran. El runner externo sí las
ejecutó y encontró una regresión real: las cuatro terminaban en `0xC0000005`, en O0/O2 y con ambos
linkers.

La causa era el cleanup automático añadido por error a la asignación de campos `owned T[]`.
Contenedores como `List`, `ByteBuffer` y `StringBuilder` ya cumplen el contrato explícito
`delete[] campo; campo = reemplazo;`; la segunda destrucción interpretaba el encabezado del array
liberado como descriptor de clase. El cleanup automático se retiró sólo para campos array;
`raii T[]` conserva su liberación local.

**Certificación posterior a la corrección:** G00 `20260906-021445-7c15dc93` terminó `LANDED`,
`certified: true`, con el mismo SHA-256 para `built_nsc` e `installed_nsc`
(`6d93b1645af25b85a0e5a92133ba1a5801ff2a621e0f368c063e7f5cab13e934`). Sus 12 etapas pasaron;
ejecutó 308 casos, sin fallos ni omitidos, incluidos CTest, fixtures y bug triage con lld-link y
NSLink en O0/O2. `c25`, `c26`, `c27` y `c28` devolvieron 42 en las cuatro combinaciones.

## Después de 0.8.0-alpha

Después del baseline apareció `NSC-LIMIT-022` en el consumidor: un
`Dictionary<char, FontChar>` retornado por un proyecto recibía una identidad de especialización
distinta al cruzar al proyecto de pruebas, aunque la forma textual del tipo fuera idéntica. Era un
defecto de identidad canónica/ABI del compilador, no de la lógica del juego ni una razón para
cambiar su API.

La causa estaba en `ProjectGenericModel`: para separar homónimos, la clave persistente incorporaba
el módulo del sitio donde se escribía el argumento. Esa regla confundía procedencia sintáctica con
identidad nominal. `FontChar` escrito en Saru y el mismo `FontChar` escrito en el proyecto de
pruebas resolvían a una sola declaración, pero producían hashes y nombres especializados distintos.

La corrección añade una predeclaración nominal ligera antes de monomorfizar, resuelve el argumento
con la misma escala de nombres y visibilidad entre proyectos y fija la clave de la declaración en
el nodo de tipo. Esa clave se conserva al clonar y sustituir una plantilla. Los tipos homónimos de
dos consumidores continúan separados porque sus declaraciones incluyen módulos distintos.

El repro de tres proyectos pasó de dos especializaciones incompatibles a una sola:

```text
Limit022GenericLib::Limit022GenericLib.Carrier`1<type:Limit022Producer::Saru.FontChar>
```

La prueba de regresión llega hasta objeto COFF y la suite Stage 52 pasa 138/138. El ejecutable
mínimo devuelve 65 tanto en O0 como en O2 con `lld-link` y con `NSLink`; `Saru.Core.Tests` vuelve a
verificar sus 7,145 callables. Los 128 ejecutables de prueba construidos para la entrega pasan; la
única negativa inicial fue el sandbox impidiendo a .NET leer `NuGet.Config`, y la repetición con
acceso normal pasó sus 56/56 comprobaciones. El baseline anterior sigue siendo evidencia válida de
la entrega anterior, pero el cambio deliberado de identidad y mangling exige una nueva corrida G00
con ambos linkers antes de promover a `0.8.0`.

Después siguen metadata de defaults y estabilización. Calendario/hora local pertenece a
Core/plataforma; finalización de estado estático sólo se reconsiderará si puede ofrecer ownership
real. Ninguno de esos puntos abre la puerta a web, SQL o enterprise: no son el propósito de
NativeSharp.

## Compatibilidad fuente C# útil, sin convertir NativeSharp en .NET

La revisión posterior del port separó seis formas que parecían una sola carencia:

- `static class` ya es una construcción real del lenguaje. El marcador llega al modelo nominal;
  una clase estática no puede declarar bases, ser instanciada, servir como base ni contener estado
  o métodos de instancia. También funciona como tipo anidado.
- Los inicializadores de auto-propiedades (`T X { get; set; } = valor;`) reutilizan el backing
  field y toda la ruta certificada de inicialización de campos. Funcionan para almacenamiento de
  instancia y estático; no se aceptan sobre propiedades con accessors explícitos o abstractos.
- `new` explícito en miembros conserva la intención de ocultamiento. En métodos no virtuales deja
  intacto el slot heredado; en `new virtual` abre un slot independiente. La omisión accidental de
  `override` sigue diagnosticándose.
- `String.Empty` existe como propiedad estática prestada sobre el literal canónico vacío. No crea
  un campo `static owned`, no necesita finalizador de módulo y no debilita ownership.
- Los argumentos con nombre ya se resuelven contra el nombre real de cada parámetro en cada
  candidata. Funcionan en métodos estáticos, de instancia y virtuales, en `new T(...)` y en
  `this(...)`/`base(...)`; pueden dejar huecos cubiertos por valores por defecto y conservan
  `ref`/`out`/`in` y ownership. Las expresiones se evalúan una sola vez y en orden fuente; sólo
  sus valores se permutan al orden ABI después de evaluarlas. La fixture C51 lo ejecuta en O0 y
  O2, incluyendo una llamada escrita en orden `tercero, primero, segundo` y un constructor
  escrito en orden `segundo, primero`.

Los “lectores/escritores de flujo de .NET” no son una obligación del lenguaje. Core ya ofrece
`BinaryReader`, `BinaryWriter`, `ByteBuffer`, `TextWriter`, `ConsoleTextWriter`,
`StringTextWriter` y `FileTextWriter` con ownership y errores nativos. No se copiará la jerarquía
de `System.IO` sólo por paridad nominal; se añadirá una pieza concreta cuando un consumidor de
juegos, assets, compilación o plataforma demuestre que falta.

**Validación del árbol actual:** 296/296 pruebas pasaron dentro del entorno restringido y la única
prueba excluida allí, el host C# por P/Invoke, pasó fuera de ese aislamiento. Total lógico:
297/297. Esto valida el código fuente; todavía no sustituye la certificación G00 del compilador
instalado.
