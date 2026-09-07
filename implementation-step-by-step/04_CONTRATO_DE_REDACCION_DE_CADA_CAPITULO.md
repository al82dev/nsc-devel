# 04 — Contrato de redacción de cada capítulo

## 1. Objetivo

Este contrato impide que la obra se degrade en resúmenes, changelogs o bloques de código sin
explicación. Todos los capítulos técnicos deberán permitir tres actividades:

1. entender por qué apareció el cambio;
2. implementarlo sobre el checkpoint anterior;
3. demostrar que el nuevo checkpoint cumple exactamente el contrato anunciado.

## 2. Unidad de avance

La unidad no será “un stage” cuando el stage sea demasiado grande. Será un **delta enseñable y
verificable**. Un delta válido tiene:

- un estado de entrada reproducible;
- un solo problema central;
- una decisión localizable;
- un conjunto revisable de archivos;
- una prueba que cambia de rojo a verde;
- un estado de salida útil por sí mismo.

Ejemplo: “implementar el lexer” es demasiado grande. “El archivo vacío produce exactamente un token
EOF con rango half-open en offset cero” es un delta válido.

## 3. Secciones obligatorias

### 3.1 Identidad del capítulo

Debe incluir:

```yaml
capitulo: 000
titulo: "..."
stage_historico: "... o no aplica"
version_producto: "..."
version_lenguaje: "..."
native_abi: "..."
runtime_abi: "..."
nsmeta: "..."
checkpoint_entrada: "Cxxx"
checkpoint_salida: "Cyyy"
estado_historico: CERTIFICADO | OBSERVADO | CORROBORADO | DOCUMENTADO | RECONSTRUIDO | PLANEADO
```

### 3.2 Qué sabe ya el lector

Lista exacta de piezas disponibles. Está prohibido usar en el capítulo una clase, helper, feature
del lenguaje o herramienta que todavía no haya sido introducida, salvo en una caja claramente
marcada como vista futura.

### 3.3 Problema observable

Debe poder reproducirse. Ejemplos:

- el driver no sabe a qué archivo pertenece un offset;
- `a + b * c` produce un árbol incorrecto;
- un phi no domina un uso;
- una closure cruzando DLL pierde su destructor;
- el gate prueba un binario distinto del instalado.

El capítulo comenzará mostrando el síntoma, no anunciando la solución.

### 3.4 Restricciones y no-objetivos

Define qué no se resolverá todavía. Esto protege la evolución incremental. Por ejemplo, el primer
`SourceManager` no necesita Unicode normalization; el primer class model no necesita inheritance;
el primer NSMETA no necesita transportar generics públicos.

### 3.5 Alternativas consideradas

Para decisiones con impacto arquitectónico se mostrarán al menos:

- alternativa adoptada;
- alternativa descartada;
- costo inmediato;
- consecuencia futura;
- evidencia que hizo preferible una.

No se inventarán debates. Si las conversaciones no muestran alternativas, se dirá que no están
documentadas y se limitará el análisis a consecuencias observables.

### 3.6 Diseño mínimo

Antes del código se fijarán:

- datos que deben existir;
- ownership/lifetime;
- invariantes;
- API pública del incremento;
- errores representables;
- relación con capas anteriores y posteriores.

### 3.7 Primer test rojo

Todo incremento implementable comienza con un test que falla por la razón correcta. Se incluirá:

- archivo completo;
- comando;
- salida relevante;
- por qué el fallo prueba la ausencia y no un error de entorno.

### 3.8 Construcción paso a paso

Cada modificación se presenta en orden:

```text
Paso N
  problema local
  archivo antes
  cambio mínimo
  archivo después o diff
  explicación línea por línea cuando sea necesario
  build/test inmediato
  nueva capacidad
  limitación restante
```

Los archivos completos se incluirán cuando sean pequeños o necesarios para reconstrucción. En
archivos grandes se incluirán diffs acumulativos más un snapshot descargable/local en la carpeta.

### 3.9 Recorrido de datos

Debe seguir un ejemplo concreto por todas las capas afectadas. Para una feature de lenguaje:

```text
source exacto
tokens exactos
AST exacto
binding y tipo
HIR
MIR
LLVM si aplica
objeto/metadata si aplica
resultado o diagnóstico
```

Para tooling:

```text
input
normalización
modelo interno
decisión
artefacto
verificación independiente
```

### 3.10 Casos negativos

Un capítulo sin casos negativos documenta la ruta feliz, no el contrato. Debe incluir malformed
input, límites y al menos una prueba que protege contra diagnostic cascades o aceptación silenciosa
cuando aplique.

### 3.11 Fallos reales encontrados

Los intentos fallidos no se limpiarán de la narración. Cada uno se describirá con:

- síntoma;
- hipótesis inicial;
- medición;
- root cause;
- fix;
- regression test;
- lección generalizable.

### 3.12 Verificación proporcional al riesgo

Se indicará qué se ejecutó:

- build incremental;
- build limpio;
- GCC/Clang/MSVC si aplica;
- warnings-as-errors;
- ASan/UBSan;
- verifier de IR;
- tests focales;
- suite completa;
- ejecución Windows/Wine;
- inspección COFF/PE;
- comparación ABI independiente;
- paquete extraído y reconstruido.

No todos los capítulos necesitan todas las filas, pero toda omisión importante debe justificarse.

### 3.13 Qué demuestra la evidencia

Ejemplo:

```text
LLVM verifier PASS
  demuestra: IR estructuralmente válido
  no demuestra: semántica correcta del lenguaje

PE verifier PASS
  demuestra: forma esperada del ejecutable
  no demuestra: que el programa ejecute el comportamiento correcto

test O0/O2 con mismo exit code
  demuestra: caso observable estable en esos niveles
  no demuestra: ausencia de UB en caminos no ejercidos
```

### 3.14 Estado exacto de salida

Árbol, nuevos comandos, features soportadas, features aún rechazadas, conteos de tests e identidad
del producto. Debe permitir empezar el capítulo siguiente sin conocimiento oculto.

### 3.15 Fuentes y confianza

Cada afirmación histórica importante enlazará su ficha `SRC-*`. Las inferencias estarán marcadas.

## 4. Longitud y densidad

No hay máximo. El mínimo lo determina la completitud de las secciones. Como regla de revisión:

- si puede leerse sin abrir código y aun así no permite reconstruirlo, falta detalle;
- si incluye miles de líneas pero no explica invariantes, sobra transcripción y falta enseñanza;
- si el lector debe adivinar el estado anterior, el capítulo no es incremental;
- si el test sólo aparece al final, se perdió la relación problema→solución;
- si una feature cruza cinco capas y sólo se documentan dos, el capítulo está incompleto.

## 5. Elementos didácticos recurrentes

Cada capítulo usará los que aporten valor:

- diagrama de memoria;
- tabla de estados;
- ejecución manual de algoritmo;
- pregunta de predicción antes del resultado;
- error intencional para el lector;
- ejercicio incremental;
- comparación con C#/C++ sólo cuando aclare el contrato;
- checklist de depuración;
- sección “si esto falla, mira aquí”.

## 6. Revisión técnica

Antes de cerrar un capítulo se comprobará:

1. que todos los nombres de archivos/símbolos existen en su checkpoint;
2. que todos los comandos funcionan desde carpeta limpia;
3. que los outputs copiados coinciden con outputs generados;
4. que las cifras de tests tienen log e identidad del binario;
5. que no hay referencias adelantadas sin marcar;
6. que los code fences están balanceados;
7. que enlaces y hashes son válidos;
8. que el capítulo siguiente puede arrancar exclusivamente desde el snapshot de salida.

