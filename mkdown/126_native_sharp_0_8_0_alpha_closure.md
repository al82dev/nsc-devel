# 126 — Native Sharp 0.8.0-alpha: cierre de los workarounds del juego

**Fecha:** 2026-09-06  
**Versión:** `nsc 0.8.0-alpha`  
**Build instalado y certificado:** `0.8.0-alpha+build.20260906T081445Z.b9aa99a37c342041`  
**Dominio:** lenguaje y compilador nativo para juegos, gráficos y plataforma.

## Decisión de versión

Esta entrega merece el salto a `0.8.0-alpha`: no es una suma de APIs, sino una mejora coherente
de semántica del lenguaje bajo un consumidor grande. El juego ya compilaba con expansiones y
ownership manual; 0.8 incorpora al compilador las formas seguras y generales descubiertas por ese
port. La identidad alpha ya está certificada; promover el texto de versión a `0.8.0` produciría
una identidad nueva y exige un último baseline, aunque no cambie la semántica.

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

El trabajo inmediato es decidir si se conserva esta entrega como `0.8.0-alpha` certificada o se
promueve a `0.8.0` y se repite el baseline de la nueva identidad. Después siguen metadata de
defaults y estabilización. Calendario/hora local pertenece a Core/plataforma; finalización de
estado estático sólo se reconsiderará si puede ofrecer ownership real. Ninguno de esos puntos abre
la puerta a web, SQL o enterprise: no son el propósito de NativeSharp.
