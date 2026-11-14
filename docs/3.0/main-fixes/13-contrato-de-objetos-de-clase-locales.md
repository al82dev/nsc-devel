# Contrato normativo: objetos de clase locales (mejora 1, M1-09)

Rama `development-main-fixes`. Resume los documentos 01, 10, 11 y 12 como contrato del lenguaje para esta
campaña.

## Semántica

`T value;` (sin inicializador, con `T` una clase) declara un objeto de clase local:

1. **Construcción.** Se construye en almacenamiento del marco: `alloca`, descriptor y constructor por
   omisión. No usa el heap y no hay un `new` oculto. No se convierte en struct: conserva identidad, herencia,
   despacho virtual y destructor.
2. **Destrucción.** Ocurre al salir de su ámbito por cualquier salida aplicable: retorno temprano o normal,
   `break` y `continue`, y salida excepcional (`cleanuppad`). Primero corre el cuerpo del destructor; después
   se destruyen los campos `owned`, cuyos hijos viven en el heap. El marco no se libera.
3. **Programas aceptados.** El compilador acepta el programa solo si demuestra que el objeto no escapa de su
   ámbito. Si no puede demostrarlo, rechaza con «local class object has an unproven use or receiver effect» o
   con «local class construction or destruction is not proven safe». Nunca cambia en silencio a heap.

## Usos demostrados

- **Acceso a campos escalares** (documento 01 y lista blanca previa).
- **Llamada a un método sobre el local.** El destino efectivo (resuelto por tipo exacto si es virtual) debe
  cumplir el resumen de escape del receptor (documento 10). Se permiten cualquier argumento y cualquier
  resultado.
- **Paso del local por valor** con conversión identidad a un callable resuelto, no virtual y de fuente, cuyo
  parámetro no escapa (documento 11). Los documentos 16 y 18 añadieron después la conversión a
  base e interfaz (ver «Aceptado después de este contrato»).
- **Alias prestado directo** en su ámbito (prueba previa).

## Construcción y destrucción demostradas

- **Constructores.**
  - Constructor por omisión explícito con cadena explícita sin argumentos, o cadena implícita completa
    (documento 12).
  - Hijos `owned` creados con `campo = new T();` (documento 01).
- **Destrucción.** La cadena de destructores debe ser de fuente y sin `throws` ni suspensión.

## Rechazado por contrato (conservador)

Esta sección se corrigió después de los documentos 14 a 18, que aceptaron formas que aquí figuraban como
rechazadas. La lista original se conserva al final como historial. Cada punto nombra el caso de
`nsc.class-value-origins` (`tests/unit/ClassValueOriginsTests.cpp`) que lo comprueba.

### Sigue rechazado

- Retornar el local o un alias prestado de él (`LocalBorrowReturn`, `LocalConditional`).
- Guardarlo: en un estático o campo (`LocalSiblingEscape`), o dentro del llamado al que se pasa
  (`LocalArgumentStored`, `LocalArgumentUpcastStored`, `LocalArgumentInterfaceStored`). Copiarlo a otra
  variable no prestada (`LocalAlias`).
- Capturarlo en una lambda, directamente o por un alias prestado (`LocalLambda`, `LocalBorrowCapture`).
- Pasarlo como argumento a un método virtual (`LocalArgumentVirtualCallee`). Un método de interfaz como
  llamado cae en la misma regla (el llamado debe resolverse de forma estática), sin caso propio.
- Un método o una implementación alcanzada que publica `this`, lo retorna, retorna un campo de referencia o
  guarda un campo no escalar en un argumento (`LocalReceiverPublishesThis`, `LocalReceiverReturnsThis`,
  `LocalReceiverReturnsReferenceField`, `LocalReceiverStoresFieldInArgument`,
  `LocalVirtualCallOnThisOverrideEscapes`, `LocalArgumentUpcastOverrideEscapes`,
  `LocalArgumentInterfaceImplementationEscapes`).
- En un constructor que lanza: publicar `this` o usar una condición con llamadas
  (`LocalThrowingConstructorPublishesThis`, `LocalThrowingConstructorCallCondition`).
- Una excepción que lleva `this` (`LocalThrowingCallExceptionCarriesThis`).
- Declarar el local dentro de un `finally` (`LocalDeclaredInFinally`).
- Usar el local como receptor o argumento de una llamada que puede suspenderse (`LocalAsync`,
  `LocalPassedToAsyncCallee`).
- En el `new` de un hijo `owned`: argumentos que son llamadas o estáticos con inicializador perezoso
  (`LocalOwnedFieldInitializerCallArgument`, `LocalOwnedFieldInitializerLazyStaticArgument`).
- Un destructor con `throws` o con suspensión (documento 14; sin caso propio).

### Aceptado después de este contrato

- **Conversión a base o interfaz como argumento** de un llamado resuelto de forma estática, cuyo parámetro
  no escapa. Las llamadas virtuales y de interfaz dentro del llamado se resuelven por el tipo exacto del
  local (documentos 16 y 18; `LocalArgumentUpcast`, `LocalArgumentInterface`). Fuera de esa forma, la
  conversión no se acepta.
- **Constructor con `throws`** que cumple las condiciones del documento 17: `throw new X(args)` o
  `if (cond) { throw new X(args); }` con argumentos escalares cerrados y condición acotada
  (`LocalThrowingConstructor`).
- **Llamadores que declaran error**, con llamadas y llamadas que lanzan sobre el local (documento 17;
  `LocalInThrowingCallerWithCalls`, `LocalThrowingCallOnLocal`, `LocalExceptionalUnknownCall`).
- **Locales en funciones async, iteradores y generadores async** (documento 18; `LocalInAsyncAcrossAwait`,
  `LocalInIteratorAcrossYield`).
- **Argumentos en el `new` de un hijo `owned`:** literales, expresiones acotadas de literales, parámetros
  escalares y estáticos escalares con inicialización trivial (documentos 14 y 16).

Los argumentos del constructor del propio local no son un rechazo del contrato sino un límite de la
sintaxis: `T value;` no admite argumentos y siempre usa el constructor por omisión.

Una salida del cuerpo de un `catch` por excepción (relanzar, lanzar otra o una llamada que lanza) omitía a
-O1 y superiores la destrucción de los locales declarados antes del `try`; el documento 19 corrige esa
generación de código para objetos de marco y de heap.

### Historial: lista original

Texto de la primera versión de este documento, sustituido por las dos listas anteriores:

- Retornar el local, guardarlo, capturarlo en una lambda, convertirlo a base o interfaz, o pasarlo a un
  callee virtual o de interfaz.
- Un método que publica `this` o un valor no escalar de sus campos, o que retorna un campo de referencia.
- Constructores con argumentos o `throws`.
- Llamadores que declaran error.

## Relación con la mejora 2

Un objeto local es un receptor «desconocido» para la regla de vida de referencias (documentos 07 y 08). Lo
que se guarda en sus campos se evalúa de forma conservadora.
