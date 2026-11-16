> **Aviso vigente sobre NSMeta:** El versionado de NSMeta ya no aplica. Las menciones de versiones, compatibilidad por versión y requisitos de versión de este documento son históricas y no constituyen un contrato vigente. No debe reintroducirse el versionado de NSMeta ni rechazarse su lectura o compilación por una versión; la aceptación se determina por el contenido y los contratos aplicables.

# Move-only por defecto y `copy` explícito / Move-only by default and explicit `copy`

## Español

En NativeSharp, todo `struct`, `record struct`, `variant` y tipo opaco declarado por el usuario es
move-only salvo que su declaración incluya `copy`. Copiar implícitamente desde un local,
parámetro, field o elemento addressable es un error; el consumo se escribe como `move(valor)`.
Un temporal recién construido o retornado no necesita `move`, porque no existe otro nombre desde
el cual pueda observarse después.

```nativesharp
public struct Token { public int Value; }
public copy struct Point { public int X; public int Y; }

Token first;
Token second = move(first); // first queda moved-from
second = new Token();       // la reasignación rehabilita second

Point a;
Point b = a;                // permitido: Point satisface Copy
```

`copy` se valida transitivamente. Un tipo no puede prometer Copy si contiene un field `owned` o
un field cuyo tipo tampoco satisface Copy. Los arreglos fijos heredan la capacidad de su elemento,
y `where T : copy` consulta exactamente la misma regla canónica.

| Fuente | Destino consumidor | Regla |
|---|---|---|
| local/parámetro/field/elemento Copy | local, parámetro o return | copia implícita permitida |
| local/parámetro move-only | local, parámetro, constructor o return | requiere `move(...)` |
| temporal move-only | local, parámetro, constructor o return | permitido sin `move` |
| valor moved-from | cualquier lectura | error hasta una reasignación completa |
| arreglo fijo `T[N]` | copia por valor | permitido sólo si `T` es Copy |

Los movimientos parciales de fields y variants, sus drop flags y su reinicialización pertenecen a
`NS3-T06-F02`; F01 no los simula ni relaja su seguridad.

### Uso de valores move-only

Al usar valores move-only en NativeSharp:

1. marque `copy` únicamente en valores pequeños y realmente copiables;
2. use `move(nombre)` cuando una API por valor deba consumir un valor move-only;
3. no lea el nombre movido hasta asignarle un valor completo nuevo;
4. cambie constraints `where T : copy` sólo cuando la API de verdad requiera duplicación.

## English

In NativeSharp, every user-declared `struct`, `record struct`, `variant`, and opaque value type is
move-only unless its declaration includes `copy`. An implicit copy from an addressable local,
parameter, field, or element is rejected; consuming the value is written as `move(value)`. A fresh
construction or returned temporary needs no `move`, because there is no source name that could be
observed afterwards.

The `copy` promise is checked transitively. A type cannot claim Copy when it contains an `owned`
field or a field whose type is not Copy. Fixed arrays inherit their element capability, and
`where T : copy` uses that same canonical rule. Partial field/variant moves and drop flags remain
the scope of `NS3-T06-F02`.

## Binary contract / Contrato binario

NSMETA carries the explicit Copy capability, and a consumer reconstructs the `copy` declaration
before semantic analysis. Copy affects source semantics, not Windows x64 layout or calling
convention. Metadata is validated by content, not by version.
