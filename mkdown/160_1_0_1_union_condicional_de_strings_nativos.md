# 160 — NativeSharp 1.0.1: unión condicional de strings nativos / Native string conditional join

## 1. Motivo del cambio

Una expresión condicional con una rama `string-literal` y otra rama `builtin:string` era
rechazada aunque la conversión implícita del literal a string nativo ya existía:

```nativesharp
string selected = condition ? "" : text;
```

El defecto no estaba en la representación de `String` ni en la evaluación perezosa del operador.
El análisis de tipos del condicional sólo aceptaba ramas idénticas o una unión numérica y nunca
consultaba la conversión específica que el lenguaje ya define para un literal de string.

## 2. Archivos y funciones modificados

### Compilador

- `src/Frontend/ProjectMethodBodyModel.cpp`
  - análisis de `ConditionalExpression`;
  - reconoce únicamente el par `string-literal`/`builtin:string` en cualquiera de los dos órdenes;
  - fija `builtin:string` como tipo resultante y conserva la combinación de ownership existente.

### Pruebas

- `tests/language/l1_conditional/Main.ns`
  - agrega ambos órdenes del condicional y comprueba el resultado ejecutable.
- `tests/unit/Stage57LanguageTests.cpp`
  - conserva un caso rojo entre string e entero para impedir que la regla se transforme en una
    unión general de tipos de referencia.

### Contrato

- `spec/NativeSharp_Language_Specification_1_0.md`
  - documenta la unión estrecha entre literal y string nativo bajo `NS-EVAL-001`.

## 3. Decisión de implementación

Se reutilizó la conversión nativa existente en vez de introducir el algoritmo de *best common
type* de C#. NativeSharp necesita resolver este par concreto para su `String`; no necesita una
jerarquía general de conversiones de referencia. La decisión mantiene pequeño el contrato y evita
aceptar accidentalmente ramas incompatibles.

No se modificó el orden de evaluación: sólo se baja la rama elegida. Tampoco se añadió `??`, tipos
nullable ni otra sintaxis de compatibilidad con C#.

## 4. Verificación realizada

- `Stage57LanguageTests`: 29 comprobaciones aprobadas con LLD y 29 con NSLink.
- `Stage60LanguageContractTests`: aprobado con ambos enlazadores en la fotografía medida.
- prueba ejecutable combinada: salida 42 en O0 y O2;
- prueba con el compilador instalado: salida 42 en O2.

Estas fueron pruebas enfocadas. No se presentaron por sí solas como un baseline certificado.

## 5. Relación con el historial

El código entró en `5a9ed611` junto con otras dos correcciones, pero aquel commit no incluyó un
acta independiente para esta semántica. Este documento repara esa omisión sin atribuirle al commit
anterior evidencia que no produjo.

## 6. Commit de referencia / Reference commit

### Título en español

`Documenta la unión condicional de strings nativos`

### English title

`Document native string conditional joins`

### Descripción en español

Registra el defecto, la regla estrecha `string-literal`/`builtin:string`, los archivos y funciones
modificados, la decisión de no introducir una unión general al estilo C# y la evidencia enfocada
que acompañó la implementación incluida en `5a9ed611`.

### English description

Record the defect, the narrow `string-literal`/`builtin:string` rule, the modified files and
functions, the decision not to introduce a general C#-style join, and the focused evidence that
accompanied the implementation included in `5a9ed611`.
