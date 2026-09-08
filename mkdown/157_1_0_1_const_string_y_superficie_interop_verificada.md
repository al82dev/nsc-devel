# 157 — NativeSharp 1.0.1: `const string` y superficie interop verificada

## Qué se midió primero

El reporte agrupaba cuatro formas necesarias para bindings Win32/Silk.NET. Sólo una era un hueco
del compilador. Un programa ejecutable devolvió 42 al probar juntas las otras tres:

| Forma | Resultado |
|---|---|
| cast explícito entero → puntero, `(ushort*)32512` | ya implementado |
| aritmética `p + 1` | ya implementada y cubierta además por `c12_pointer_arith` |
| `explicit operator` definido por el usuario | ya implementado |
| `operator ==` definido por el usuario | ya implementado |
| `public const string Name = "...";` | era el hueco real |

El probe no se quedó en parsear: convirtió `32512` a puntero, avanzó un buffer, ejecutó la
conversión de un handle tipado y comparó dos handles. Su salida fue 42.

## Por qué fallaba `const string`

Stage 37 imponía dos límites independientes. El chequeo de tipo sólo admitía constantes
integrales, booleanas y flotantes; después, el evaluador de expresiones constantes tampoco tenía
una variante para strings. Quitar únicamente el primer diagnóstico habría desplazado el error a
MIR, donde el valor validado no podía materializarse.

## Cómo queda el pipeline

1. El análisis semántico reconoce un literal `string` y referencias a otro `const string` como
   expresiones constantes.
2. MIR conserva los bytes del valor en `ProjectOopMirStaticField`, igual que ya conservaba bits
   enteros, booleanos y flotantes.
3. LLVM interna una sola representación `{ longitud, bytes con NUL }` por contenido.
4. Cada global `const string` es un `constant ptr` hacia ese literal inmutable; no hay cctor ni
   store de inicialización en runtime.

La misma rutina de internado sirve ahora a expresiones literales y a campos constantes, por lo que
ambas rutas no pueden divergir en layout.

## Evidencia permanente

`Stage37StaticInitializationTests` verifica el valor MIR de un literal y de un alias, el verifier
LLVM y la forma `constant ptr` hacia bytes inmutables. `c53_const_string` compila y ejecuta en O0 y
O2, incluyendo literal, alias y string vacío.

La especificación `NS-MEMBER-001` fija que los campos `const string` son constantes de compilación,
no campos mutables disfrazados.

## Alcance honesto

Esto resuelve la forma generada por los tres `StringConstants.ns`. No añade concatenación constante,
`const string = null` ni cambia el formato NSMETA. Esas formas no son necesarias para sustituir los
métodos estáticos actuales por los `public const string` de Silk.NET.
