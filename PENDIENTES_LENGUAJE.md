# Pendientes del lenguaje — lista de trabajo

Derivada de `78_huecos_del_lenguaje_completo.md`. Sólo core. Sin LINQ, patrones, `async`, `record`.

## Nivel 1 — un programa correcto puede fallar o falsear

| ID | Falta | Estado hoy |
|---|---|---|
| 87 | `volatile` | no existe |
| ~~79~~ | `++` `--` | **HECHO** (doc 79) |
| ~~81~~ | `+= -= *= /= %= &= \|= ^= <<= >>=` | **HECHO** (doc 79) |
| 88 | `checked` / `unchecked` | no existe |
| 89 | `sbyte` | no existe |

## Nivel 2 — bloquea escribir biblioteca

| ID | Falta | Estado hoy |
|---|---|---|
| ~~80~~ | `p->campo` | **HECHO** (doc 79) |
| ~~83~~ | `p->Metodo()` | **HECHO** (doc 79) |
| 82 | `+=` sobre propiedad/indexador | parsea; muere en sema |
| 84 | lambdas anidadas | parsea; muere en sema |
| 85 | delegado a método virtual | parsea; muere en sema |
| 86 | delegado con receptor struct | parsea; muere en sema |
| 90 | restricciones genéricas `where` | parsea; rechazado |
| 91 | interfaces genéricas | no existe |
| 92 | tipos anidados | no existe |
| 93 | constructores estáticos | no existe |
| 94 | `enum E : byte` | no existe |
| 95 | `params` | no existe |
| 96 | parámetros opcionales | no existe |
| 97 | argumentos con nombre | no existe |
| 98 | métodos de extensión | no existe |
| 99 | propiedades/indexadores sólo de escritura | rechazado |
| 100 | atributos `[Attr]` | no existe |
| 101 | `goto` y etiquetas | no existe |

## Nivel 3 — comodidad

| ID | Falta |
|---|---|
| 102 | interpolación `$"…"` |
| 103 | verbatim `@"…"` |
| 104 | `??` `??=` |
| 105 | `?.` |
| 106 | `is T x` (variable de patrón) |
| 107 | `when` en `catch` |
| 108 | `new T { X = 1 }` |
| 109 | miembros `=> x` |
| 110 | `int[,]` |
| 111 | `for` con varios inicializadores/incrementos |
| 112 | `typeof` |
| 113 | `nameof` |
| 114 | separadores `1_000` |
| 115 | escapes `\u` `\U` |
| 116 | preprocesador `#if` |
| 117 | `object` `decimal` como palabras |
| 118 | `lock` (hay `raii ScopedLock`) |

## Abiertos de antes, no de lenguaje

| ID | Falta |
|---|---|
| 77 | los logs no dicen cuántas comprobaciones corrió una suite |
| 78 | `enum` mide 8 en `valueLayoutForTypeKey` y 4 en `layoutNamedType` — sin medir |
