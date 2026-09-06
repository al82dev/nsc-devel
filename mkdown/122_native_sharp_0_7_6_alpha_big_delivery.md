# 122 — Native Sharp 0.7.6-alpha: entrega grande de corrección

**Fecha:** 2026-09-05
**Lenguaje:** NativeSharp 0.1
**Compilador validado:** `nsc 0.7.6-alpha+build.20260906T053548Z.4f4b5b753a39bc37`
**Stage:** 57

## Resultado

Esta entrega no añade un dominio nuevo al lenguaje. Cierra contradicciones dentro de la promesa
existente: ownership determinista, operadores de bajo nivel tipados, nombres familiares para un
programador C# y composición entre proyectos.

Quedan corregidos:

- `NSC-LIMIT-011`: constraints genéricos con tipos definidos por el consumidor;
- `NSC-LIMIT-012`: bitwise entre valores del mismo enum;
- `NSC-LIMIT-013`: campo y tipo importado con nombres que colisionan;
- `NSC-LUNAR-007`: shift con valor `ulong` y contador `int` que antes producía LLVM inválido;
- `NSC-LIMIT-015`: reemplazo de campo `owned` cuyo RHS lee el owner anterior;
- `NSC-LIMIT-019`: referencia calificada y transitiva entre campos `const`.

La identidad pública deja de decir `0.7.4-alpha` y pasa a `0.7.6-alpha`. El salto reúne el baseline
0.7.5 ya certificado y el P0 de ownership que define 0.7.6, además de cuatro correcciones que se
pudieron cerrar con reglas completas durante la misma ronda. No se llama 0.8: todavía quedan
riesgos estructurales de lifetime, especialización y equivalencia O0/O2.

## Reglas que quedaron fijadas

### Reemplazo de owners

Una asignación a storage propietario sigue este orden:

1. obtiene una dirección estable del destino;
2. evalúa completamente el RHS;
3. destruye el owner desplazado;
4. almacena el valor nuevo.

Esto hace válida la forma natural:

```csharp
text = String.Concat(text, value);
```

El RHS nunca observa un objeto que el propio store acaba de destruir. La regresión usa tanto una
clase con contadores de destrucción como un `owned string`, en O0 y O2.

### Enums como máscaras tipadas

`E | E`, `E & E`, `E ^ E` y `~E` producen `E`. Las asignaciones `|=`, `&=` y `^=` conservan la
misma regla en locales, campos y propiedades. Un enum no obtiene por ello conversiones numéricas
implícitas generales: mezclar dos enums distintos o enum con entero sigue siendo error.

### Shifts y representación LLVM

El tipo del resultado continúa siendo el del operando izquierdo y el contador conserva su tipo
semántico. Inmediatamente antes de emitir `shl`, `lshr` o `ashr`, el backend normaliza el contador
al ancho físico que LLVM exige. Así, `ulong << int` deja de formar `shl i64 ..., i32 ...` sin
inventar una promoción visible en el lenguaje. La misma ruta sirve a `<<=`, `>>=` y shifts a la
derecha.

### Declaración frente a expresión

Que un identificador exista también como nombre de tipo no basta para convertir una sentencia en
declaración. Una declaración requiere la forma sintáctica `tipo identificador`. Por eso pueden
coexistir un tipo `GL`, un tipo importado `Raw.Gl`, el campo `GL Gl` y la sentencia `Gl = gl;`.
Las referencias estáticas legítimas a `GL` permanecen válidas.

### Grafo de constantes

Todos los inicializadores de campos se enlazan antes de validar dependencias `const`. Después, un
conjunto de visita detecta ciclos reales. El ordinal interno —estable y ordenado por identidad de
tipo— ya no se confunde con orden de dependencia. Esto habilita referencias adelantadas,
calificadas, transitivas y cross-project sin aceptar ciclos.

## Evidencia incorporada al repositorio

- `Stage50_11OwnershipExecutionTests`: reemplazo propietario sobre clase y string, O0/O2;
- `Stage37StaticInitializationTests`: dependencia adelantada válida y ciclo inválido;
- `Stage56BugFixTests`: fixtures punta a punta para shifts, enums, colisión de nombres y constantes
  cross-project;
- fixture negativa que exige el diagnóstico de mezcla de enums distintos;
- repros externos exactos `011`, `012`, `013`, `015`, `019` y `shift-width` revalidados con el
  compilador de esta entrega. Esos proyectos sólo aportan evidencia; no forman parte ni se
  modifican desde `nsc`.

La reconstrucción completa terminó correctamente. La matriz CTest ejecutó sus 293 casos: 287
pasaron en la corrida paralela y seis emisiones `.obj` chocaron con un bloqueo transitorio de
Windows (`user-mapped section open`); repetidas inmediatamente en serie, las seis pasaron. No quedó
ningún fallo funcional persistente. Las suites dirigidas cerraron además `196/196` checks de
ownership, `58/58` de bugfix y `57/57` de constantes.

La certificación instalada de 0.7.6 exige todavía una nueva corrida G00 sobre este binario exacto.
Una certificación anterior no se transfiere a un build nuevo ni una validación local sustituye la
comparación built/installed.

## Pendiente real después de 0.7.6

No se maquillaron como cerrados los puntos que requieren una solución transversal:

- `NSC-LIMIT-021`: owner temporal prestado durante una expresión, con cleanup también en unwind;
- `NSC-LIMIT-016`: arrays `raii`, recorrido de elementos, slots movidos y unwind parcial;
- `NSC-LIMIT-017`: `new T[] { ... }`, incluida la seguridad de inicialización parcial;
- `NSC-LIMIT-014`: lifetime de `static owned`, o exclusión explícita para 1.0;
- `NSC-LIMIT-018`: especialización genérica por miembro realmente alcanzado;
- `NSC-LIMIT-005`: cierre contractual de equivalencia O0/O2;
- `NSC-LIMIT-020`: defaults completos en AST, resolución de overloads, constructores y metadata.

Aceptar únicamente la sintaxis de cualquiera de ellos ocultaría el defecto detrás del frontend.
El criterio sigue siendo punta a punta: semántica, HIR, MIR, LLVM, cleanup, ejecución y diagnóstico.

## Frontera permanente

Nada de esta entrega abre web, SQL, bases de datos, ORM, LINQ, reflection dinámica ni aplicaciones
enterprise como objetivo del lenguaje o de Core. Native Sharp sigue orientado a juegos, gráficos,
audio, input, engines, herramientas nativas y software de plataforma donde importan frame time,
layout, ABI, memoria y costos visibles. La compatibilidad con C# se usa para evitar diferencias
arbitrarias de fuente; no para copiar .NET ni sus dominios de aplicación.
