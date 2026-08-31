# `tests/bug_triage/` — ¿cuáles de los bugs reportados siguen vivos?

Ocho reproductores mínimos, uno por bug del compilador que las instancias consumidoras
(bindings, port de `Saru.Framework`, port del tutorial de OpenGL) levantaron contra `nsc` y que
**no están en `BUGS_ENCONTRADOS/` de este repo ni arreglados**.

El punto de esta carpeta no es arreglar nada. Es **medir**: varios de estos se reportaron
contra compiladores anteriores a C1, C6, C7 y C8, y puede que ya estén muertos sin que nadie
lo verificara. C7 ya mató uno de C6 de paso, sin buscarlo.

## Cómo se corre

Desde la raíz del repo:

```
run_bug_triage.bat
```

No reconstruye el compilador — usa el `nsc` instalado en `C:\native-sharp`. El resultado queda
en `_logs\TRIAGE.txt`, y el detalle completo de cada compilación y ejecución en
`_logs\10_triage.log`.

## Cómo leer el resultado

Cada caso **devuelve 42 cuando el compilador está sano**. Cualquier otra cosa es el bug:

| Lo que pasa | Significa |
|---|---|
| exit 42 | **ARREGLADO** |
| no compila / no linkea | **VIVO** — el bug es ruidoso |
| compila y devuelve otro número | **VIVO** — el bug es silencioso, que es peor |
| compila y se cae | **VIVO** — ver el código; un stack overflow sale como número enorme |

Cada fixture corre **primero sus controles** — las variantes vecinas que se sabe que funcionan —
y sólo después el caso del bug. Así, un fallo del reproductor mismo se distingue del bug: si
devuelve 1 o 2, el fixture está mal escrito; si devuelve el último número, es el bug.

Cada `Main.ns` lleva arriba el reporte original: qué se rompió, dónde se encontró, qué impacto
tuvo medido, y por qué el control de al lado sí funciona.

## Los ocho

| # | Carpeta | Bug | Severidad |
|---|---|---|---|
| 1 | `b1_this_copy` | `T local = this;` en un método de struct guarda el **puntero**, no una copia | **alta — silencioso** |
| 2 | `b2_operator_null` | `x == null` dentro de un `operator ==` recursa infinitamente | alta — cuelga |
| 3 | `b3_ctor_aggregate` | un constructor no recibe un struct de más de 8 bytes por valor | media |
| 4 | `b4_this_by_value` | pasar `this` por valor desde un método de struct rompe el verifier | media |
| 5 | `b5_ctor_owned_field_array` | `new V3(...)` a un elemento de arreglo `owned` que es campo | media — error interno sin ubicación |
| 6 | `b6_delegate_projectref` | un `extern C delegate` no cruza un `ProjectReference` | media |
| 7 | `b7_duplicate_extern_c` | el mismo símbolo C en dos clases no linkea (`símbolo.1`) | probablemente ya cerrado por C1 |
| 8 | `b8_shift_folding` | `1u << 28` pliega a `double`; y shift sobre `ushort` | baja |

`b6` es el único con dos proyectos (`lib/` + `app/`), porque el bug **es** cruzar el borde de
proyecto. El runner compila `app/`.

## Dos que no están aquí, a propósito

`sqrt se llama a sí misma` y `doble codificación UTF-8` figuran entre los bugs reportados pero
**no son del compilador**: el primero era un `.c` del archive de stb (`__builtin_sqrt` bajando a
una llamada a libm por `errno`), el segundo eran helpers `Utf8` de los bindings que codificaban
un `string` que ya era UTF-8. Los dos ya los arregló esa instancia.
