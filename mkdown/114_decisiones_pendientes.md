# 114 -- Decisiones abiertas: SHR-063, ID 125, ID 87

Fecha: 2026-09-03
Estado: **decididas provisionalmente por juicio tecnico, con el visto bueno de Alberto
("confio en tu juicio por ahora")**. Ninguna esta implementada. Cada una lleva escrita su
CONDICION DE REAPERTURA, que es lo unico que las convierte otra vez en pregunta.

**Por que existe este documento.** Las tres llevan abiertas desde el doc 61 y en cada cierre de
bloque se repite la misma linea: *"siguen siendo decisiones tuyas, no de ingenieria"*. Una decision
que se aplaza indefinidamente no es prudencia: es una decision tomada por omision y sin escribir.
Esto la escribe.

---

## 1. SHR-063 -- condicionales de compilacion (`#if`)

**Decision: NO, todavia no.**

### Que se pedia

Un mecanismo para compilar codigo distinto segun la configuracion -- lo que en C y C# hace el
preprocesador con `#if DEBUG`.

### Por que no

**No hay consumidor.** El unico que aparecio en toda la revision es el modo de diagnostico de
overflow del doc 110 §3: insertar una comprobacion en Debug que aborte al desbordar. Y ese caso
**no necesita sintaxis**: es una propiedad de proyecto que el compilador lee y una decision de
emision en el backend. El programa se escribe una sola vez y el compilador emite una cosa u otra.

**Y un preprocesador se paga para siempre.** Deja de haber un solo texto de programa: hay tantos
como combinaciones de simbolos. Eso golpea a todo lo que lee codigo -- el propio compilador, las
herramientas, el `SourceManager` que ahora mismo hace falta enhebrar para dar
`fichero:linea:columna` (doc 112). Un `#if` mal cerrado da errores en la linea equivocada, y las
ramas que nadie compila se pudren sin que ninguna prueba lo note.

**El criterio del proyecto ya lo dice**, y es el mismo que se aplico a `checked`/`unchecked` en el
doc 110 §3 y a `volatile` en el 111 §2.5: *solo si dos comportamientos dentro de un mismo programa
tienen un consumidor real se agrega sintaxis para seleccionarlos.*

### Condicion de reapertura

Cualquiera de estas dos, y basta con una:

- Aparece codigo que tiene que **existir o no existir** segun la plataforma o la configuracion, y no
  solo comportarse distinto. Un `if` normal no sirve ahi porque la rama muerta tiene que compilar.
- El proyecto empieza a tener un segundo objetivo (ARM64, Linux) y hay declaraciones que solo tienen
  sentido en uno.

Lo que NO es motivo para reabrir: querer trazas de depuracion (eso es una funcion vacia que el
optimizador quita), ni querer el modo de diagnostico de overflow (es una propiedad de proyecto).

---

## 2. ID 125 / SHR-079 -- UTF-8

**Decision: `string` es BYTES OPACOS. UTF-8 es la codificacion, no la semantica.**

### Que significa en concreto

- `s.Length` cuenta **bytes**. Es O(1).
- `s[i]` da el **byte** i-esimo, de tipo `char` (8 bits, sin signo).
- Cortar, concatenar, comparar e indexar trabajan sobre bytes y no miran el contenido.
- Lo que de verdad necesita caracteres --recorrer puntos de codigo, comparar ignorando
  mayusculas, contar "caracteres"-- va en **funciones aparte que lo digan en el nombre**, y son las
  unicas que pagan el coste de decodificar.
- El compilador y la biblioteca tratan los ficheros fuente y las cadenas literales como UTF-8: eso
  ya es cierto hoy y no cambia.

### Por que

**Porque ya es asi de hecho, y lo contrario cuesta caro.** `builtin:char` mide ocho bits y no tiene
signo -- esta fijado con un `static_assert` en `BuiltinIntegerTypes.h` desde el CC1-100. Un `string`
de NativeSharp es `{i64 longitud; bytes}`. Hacer que `.Length` cuente caracteres obligaria a
**decodificar en cada indexacion**: O(n) donde hoy es O(1). En un compilador que lexa ficheros y en
un juego que arma texto por cuadro, eso no es un detalle.

**Y es lo que hacen los lenguajes que tomaron la decision tarde y con datos.** Go y Rust: los bytes
son bytes, y la iteracion por puntos de codigo se pide explicitamente. C# hizo lo contrario --UTF-16
con `.Length` en unidades de codigo-- y el resultado es que `.Length` **tampoco da caracteres**
(los emoji ocupan dos), asi que se paga el coste sin obtener la garantia.

**La honestidad del nombre importa.** Un `.Length` que promete caracteres y entrega otra cosa es
peor que uno que promete bytes y entrega bytes.

### Lo que esta decision NO dice

- No dice que la biblioteca no vaya a tener utilidades UTF-8. Dice que **no son el comportamiento
  por omision de los operadores del lenguaje**.
- No dice nada sobre UTF-16 en la frontera con Win32. Las APIs `W` de Windows toman UTF-16 y ese es
  un problema de conversion en el borde, no del tipo `string`. Va con el hallazgo de
  `demo_ventana`: hoy no existe ni siquiera la conversion a cadena C terminada en NUL.

### Condicion de reapertura

- Si aparece un consumidor que **indexa por caracteres en un bucle caliente** y las funciones
  explicitas resultan incomodas de verdad, no solo mas largas de escribir.
- Si el proyecto adopta una API de texto que exige puntos de codigo en su superficie.

---

## 3. ID 87 -- `volatile`

**Decision: ESPERAR. Desbloqueado, no implementado.**

El doc 111 §2.5 ya escribio el contrato completo --que `volatile` impide eliminar, fusionar y
reordenar accesos a una posicion observable, y que **no es sincronizacion**-- asi que SHR-075 ("no
implementar keyword sin contrato") ya no bloquea.

Lo que falta es lo otro: **un consumidor**. Un `volatile` sin memoria mapeada que tocar es una
palabra clave que nadie usa, y una palabra clave que nadie usa es una que nadie prueba.

### Condicion de reapertura

Aparece MMIO, un registro de dispositivo, o un binding que lo necesite por escrito.

---

## 4. Resumen

| | Decision | Quien la reabre |
|---|---|---|
| SHR-063 `#if` | no, todavia no | codigo que deba EXISTIR o no, o un segundo objetivo |
| ID 125 UTF-8 | bytes opacos, `.Length` en bytes, O(1) | un consumidor que indexe por caracteres en un bucle caliente |
| ID 87 `volatile` | esperar | MMIO o un binding que lo pida |

Las tres son **provisionales por juicio tecnico**, no cerradas. Alberto puede revertir cualquiera
sin que nada de lo construido hasta hoy tenga que rehacerse: ninguna esta implementada y ninguna
tiene codigo que dependa de ella.
