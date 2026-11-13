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
  parámetro no escapa (documento 11).
- **Alias prestado directo** en su ámbito (prueba previa).

## Construcción y destrucción demostradas

- **Constructores.**
  - Constructor por omisión explícito con cadena explícita sin argumentos, o cadena implícita completa
    (documento 12).
  - Hijos `owned` creados con `campo = new T();` (documento 01).
- **Destrucción.** La cadena de destructores debe ser de fuente y sin `throws` ni suspensión.

## Rechazado por contrato (conservador)

- Retornar el local, guardarlo, capturarlo en una lambda, convertirlo a base o interfaz, o pasarlo a un
  callee virtual o de interfaz.
- Un método que publica `this` o un valor no escalar de sus campos, o que retorna un campo de referencia.
- Constructores con argumentos o `throws`.
- Llamadores que declaran error.

## Relación con la mejora 2

Un objeto local es un receptor «desconocido» para la regla de vida de referencias (documentos 07 y 08). Lo
que se guarda en sus campos se evalúa de forma conservadora.
