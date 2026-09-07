# 131 — Bitácora por cambio independiente de Git

## Qué problema se corrigió

La documentación reciente podía reconstruir una entrega apoyándose demasiado en los commits.
Ese método pierde información cuando el trabajo todavía no se ha confirmado, cuando una entrega
grande acumula varios cambios o cuando el commit describe el resultado pero no la investigación
que condujo a él.

Una lista como «se modificaron seis archivos» tampoco enseña qué cambió dentro del compilador.
No dice qué comportamiento existía antes, cuál era la causa, por qué se eligió una solución ni
qué función cumple cada archivo después de la corrección.

## Decisión permanente

Desde esta entrega, cada cambio de NativeSharp se acompaña de un Markdown propio o de una sección
nueva claramente delimitada dentro del Markdown de la entrega activa. La documentación se escribe
junto con la implementación y no se pospone hasta que exista un commit.

El commit, si aparece, sirve como ancla reproducible. No sustituye la explicación.

## Cómo se narrará cada cambio

La explicación seguirá el recorrido real del trabajo:

1. **Situación inicial:** qué hacía el compilador y cómo se observó.
2. **Síntoma:** ejemplo, diagnóstico, IR inválido, fallo de enlace o comportamiento incorrecto.
3. **Causa:** regla ausente o implementación concreta responsable.
4. **Diseño:** solución elegida, invariantes que debe conservar y alternativas consideradas.
5. **Implementación:** capas recorridas y responsabilidades incorporadas.
6. **Inventario comentado:** archivos y funciones nuevos o modificados, explicando para qué sirven.
7. **Resultado observable:** diferencia antes/después.
8. **Pruebas:** qué caso se agregó y qué afirmación demuestra.
9. **Producto:** nueva identidad y hash de `nsc.exe` cuando corresponda.
10. **Frontera:** qué no resuelve todavía y cuál es el siguiente paso lógico.

Cuando un intento inicial falle, también se contará. La evidencia intermedia es parte del valor
didáctico porque explica por qué la solución final tiene esa forma.

## Qué cambió en el proyecto para establecer la regla

### Archivo modificado

`mkdown/128_regla_de_generacion_por_pasada.md`

Se añadió la sección **«Regla independiente de Git: un Markdown por cada cambio»**. Ahí queda la
obligación permanente y el contenido mínimo de cada bitácora. La regla ahora distingue de forma
explícita entre:

- la implementación, que cambia el producto;
- la bitácora didáctica, que conserva el razonamiento;
- Git, que puede identificar un estado pero no narrarlo por sí solo.

### Archivo nuevo

`mkdown/131_bitacora_por_cambio_independiente_de_git.md`

Es este documento. Funciona además como primera aplicación de la regla: el cambio documental que
la establece no queda sin su propia explicación.

### Índice actualizado

`mkdown/CONTENIDO.txt`

Se enlazó esta regla junto al documento de evolución histórica. De ese modo, quien abra el índice
encuentra primero tanto la historia reconstruida como el procedimiento obligatorio para continuarla.

## Funciones y código afectados

Ninguna función del compilador cambió. No se modificaron Parser, Sema, HIR, MIR, backend, runtime,
driver ni linker. Esta entrega cambia el método de trabajo y la trazabilidad documental.

Por esa razón no corresponde generar un `nsc.exe` nuevo ni atribuir una identidad binaria nueva.

## Garantía que queda establecida

A partir del siguiente cambio técnico, el cierre deberá responder con nombres concretos a estas
preguntas:

- ¿qué ocurría antes?;
- ¿qué se descubrió durante el trabajo?;
- ¿qué decisión se tomó y por qué?;
- ¿qué archivos cambiaron y qué papel tiene cada uno?;
- ¿qué funciones o métodos cambiaron y cómo cooperan?;
- ¿qué prueba demuestra el nuevo comportamiento?;
- ¿qué queda pendiente después de la entrega?

Así la evolución futura podrá leerse directamente desde `mkdown/` incluso si todavía no existe
un commit o si el historial de Git agrupa varias decisiones.
