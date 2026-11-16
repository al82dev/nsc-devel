# Tanda 19, cambio 1: `null` en punteros a función C

## Defecto

En la [auditoría inicial](../TANDA-19-AUDITORIA-INICIAL.md), `s.f = null`, `call(null, 3)` y `s.f == null` con
`typedef int (*cb)(int)` se rechazan (`cannot implicitly convert ... from 'builtin:null' to 'fn:ptr:c:...'`). C los
acepta con `NULL`.

Causa en el código (`Frontend/ProjectTypeConversion.cpp`):

- `classifyImplicit` consulta `classifyFunctionImplicitConversion` antes que `classifyNullImplicitConversion`.
  La primera devuelve "sin conversión" cuando el destino es una función y el origen no lo es, también para
  `null`.
- `classifyNullImplicitConversion` solo admitía un puntero a función marcado `nullable`. Un puntero importado de
  C, de convención `c`, no lleva esa marca.

## Diseño

`null` no es un valor función: `classifyFunctionImplicitConversion` no lo clasifica y decide
`classifyNullImplicitConversion`. Un puntero a función de convención extranjera (cualquier convención distinta de
la de NativeSharp) admite nulo, como en C. La comparación usa la misma clasificación. La identidad del tipo no
cambia, así que NSMeta tampoco.

## Cambio

- `ProjectTypeConversion.cpp`, en `classifyFunctionImplicitConversion` y `classifyNullImplicitConversion`.

No se modificó Clang ni NSLink.

## Evidencia

Fixture nuevo `pruebas-tanda-19-nulos`, con y sin `--dll`. Un struct C con un campo `t19_cb` recibe `null`; una
función recibe `null`; se comparan el campo y un puntero devuelto con `null` (`==` y `!=`). Resultado 11157, igual
que el control C, O0–O3.

| Variante | Evidencia |
|---|---|
| Estática | `A:/temp/nsc-t19-nulos-1v8xvx5z` |
| DLL | `A:/temp/nsc-t19-nulos-1dj5azxc` |

Rechazo genuino: `null` a un parámetro `int` se rechaza (`argument 1 of type 'null' does not convert to 'int'`).
Clang también lo rechaza (`incompatible pointer to integer conversion`).

## Límites

- Llamar a través de un puntero nulo falla en ejecución, como en C; NativeSharp no lo comprueba.
