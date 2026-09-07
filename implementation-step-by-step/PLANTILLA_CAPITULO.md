# NNN — Título del capítulo

> Estado: `BORRADOR | VERIFICADO | CERTIFICADO`  
> Confianza histórica: `CERTIFICADO | OBSERVADO | CORROBORADO | DOCUMENTADO | RECONSTRUIDO | PLANEADO`

## Ficha

```yaml
capitulo: NNN
stage_historico: ""
version_producto: ""
version_lenguaje: ""
native_abi: ""
runtime_abi: ""
nsmeta: ""
checkpoint_entrada: ""
checkpoint_salida: ""
fecha_historica_aproximada: ""
fecha_de_reconstruccion: ""
```

## 1. Qué sabe ya el lector

Inventario exacto del checkpoint de entrada.

## 2. El problema observable

Programa, comando, salida y explicación de por qué el estado anterior no basta.

## 3. Alcance

### Incluido

- ...

### Deliberadamente fuera

- ...

## 4. Evidencia histórica

| Fuente | Afirmación | Confianza | Corroboración |
|---|---|---|---|
| SRC-0000 | ... | ... | ... |

## 5. Alternativas

### Alternativa adoptada

...

### Alternativas descartadas

...

## 6. Diseño e invariantes

### Datos

...

### Ownership y lifetime

...

### Errores

...

### Invariantes

1. ...

## 7. Primer test rojo

### Fuente

```text
...
```

### Comando

```text
...
```

### Resultado anterior

```text
...
```

## 8. Implementación paso a paso

### Paso 1 — ...

#### Por qué

...

#### Archivo antes

```cpp
...
```

#### Cambio

```diff
...
```

#### Explicación

...

#### Verificación inmediata

```text
...
```

### Paso 2 — ...

Repetir la misma anatomía.

## 9. Recorrido completo del ejemplo

### Source

### Tokens

### AST

### Semántica

### HIR

### MIR

### LLVM

### Artefacto físico

### Resultado observable

Eliminar únicamente las capas que todavía no existan o que no participen, explicando por qué.

## 10. Casos negativos

| Caso | Diagnóstico esperado | Capa responsable |
|---|---|---|
| ... | ... | ... |

## 11. Fallos reales y depuración

### Síntoma

### Hipótesis

### Medición

### Causa raíz

### Corrección

### Regression test

## 12. Verificación final

| Verificación | Comando/artefacto | Resultado | Qué demuestra | Qué no demuestra |
|---|---|---|---|---|
| ... | ... | ... | ... | ... |

## 13. Estado de salida

### Árbol nuevo

```text
...
```

### Capacidades

- ...

### Limitaciones

- ...

### Identidad y hashes

```text
...
```

## 14. Ejercicios de reconstrucción

1. ...

## 15. Siguiente capítulo

Problema que ahora se vuelve visible y que justifica el siguiente delta.

