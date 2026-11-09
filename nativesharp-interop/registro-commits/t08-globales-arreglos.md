# T08-004-04c: arreglos globales C y matrices con punteros calificados

Fecha: 2026-11-08. Casilla: T08-004-04c.

## Resultado

Verificada sin cambios en el compilador.

## Prueba

Nueva: `pruebas-tanda-08-globales-c/validar-arreglos.py`. Resultado: aprobada en O0–O3, con rechazos en O0.

- Globales: `int[4]`, `int[2][3]`, `const int[3]`, `volatile int[2]`, `int *[2]`, `const char *[3]`,
  `volatile int *[2]`, `int *const [2]`, `Pair[2]`, `const Pair[2][2]` y `double[2][2][2]`.
- Comprobaciones:
  - lecturas de elementos y de campos de records en matrices;
  - lecturas a través de los punteros de los arreglos, incluidos `const char *` y `volatile int *`;
  - una fila tomada como `&g_mat[1][0]`;
  - escrituras en las mutables, que C comprueba;
  - lecturas de lo que C cambia después.
- Las mismas comprobaciones se hacen con un control Clang, directo con biblioteca estática, con datos
  `dllimport` de una DLL y desde una plantilla precompilada sin el encabezado.

| Rechazo (O0) | Diagnóstico |
|---|---|
| `g_carr[0] = 1` | «left side of assignment is not assignable» |
| `g_cptrs[0] = &x` (`int *const [2]`) | «left side of assignment is not assignable» |
| `p = g_names[0]` con `sbyte* p` | «… mutability … contract» (descarta el `const` del dato apuntado) |
| `int* p = g_vptrs[0]` | «initialization would discard pointer pointee volatile qualification» |
| `g_arr = other` | «left side of assignment is not assignable» |
| `g_arr[4]` | «constant fixed-array index is outside [0, 4)» |

## Observación

En NativeSharp, un arreglo fijo `T[N]` no decae implícitamente a `T*`: `int* row = g_mat[1];` se rechaza con
«cannot initialize local 'row' of type 'builtin:int*' from 'builtin:int[3]'». Es una regla general de los
arreglos fijos del lenguaje, no algo de la interoperabilidad. La forma explícita `&g_mat[1][0]` funciona.
El arreglo C de tamaño desconocido (`T[*]`, ver [t08-global-arreglo-incompleto](t08-global-arreglo-incompleto.md))
sí decae, como el miembro flexible.
