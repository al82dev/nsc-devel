# T08-002-09m7d: demás calificadores y variantes de enlace de la global externa compartida

Fecha: 2026-11-08.

## Prueba

Nueva: `pruebas-tanda-08-identidad-c/global-externa-variantes/validar.py`.

- Dos productores precompilados leen la misma global C externa desde una plantilla. Cada uno ve una
  declaración, y ambos se consumen sin sus encabezados. El consumidor enlaza la definición C y devuelve
  `A*1000+B`.
- Con `--observar`, imprime el resultado de cada caso, también los límites, sin comprobar.

Resultado: aprobada.

| Caso | Declaraciones A / B | Resultado |
|---|---|---|
| `const volatile int` | iguales | correcto O0–O3 |
| `int **` | iguales | correcto O0–O3 |
| `int arr[4]` | iguales | correcto O0–O3 |
| `int *restrict` | iguales | correcto O0–O3 |
| `const int *const` | iguales | correcto O0–O3 |
| `_Alignas(16) int` | iguales | correcto O0–O3 |
| `__declspec(dllimport) extern int` desde una DLL | iguales | correcto O0–O3 |
| `int arr[4]` / `int arr[5]` | distintas | rechazado: `ambiguous namespace state reference` |
| `const volatile int` / `const int` | distintas | rechazado |
| `int *restrict` / `int *` | distintas | rechazado |

Antes ya estaban cubiertos `volatile`, `const`, la calificación del dato apuntado en un nivel,
`_Atomic` y TLS (T08-002-09m1 a 09m6). No hizo falta cambiar el compilador.

## Límites encontrados (pendientes registrados)

1. `extern int open_arr[];` (arreglo de tamaño desconocido) no se importa: «C/C++ global type is not yet
   mapped», también en un proyecto directo.
2. Si B declara `extern const int **pp;` y A declara `extern int **pp;`, el consumidor no rechaza las
   declaraciones incompatibles: la plantilla de B lee la global de A y devuelve el valor. La causa
   observada es que el tipo de B no se importa (lo confirma un proyecto directo: «C/C++ global type is
   not yet mapped»). Así, la plantilla transportada de B no encuentra su propia declaración y resuelve el
   nombre con la de A. Para diagnosticarlo, el consumidor necesitaría saber que el productor tenía una
   declaración no importable de ese nombre. Hoy el catálogo C publicado no lo registra. Si exige un
   campo nuevo en NSMeta, es una decisión.
