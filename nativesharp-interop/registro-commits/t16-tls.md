# T16-006 y T16-007: TLS de bibliotecas C++ desde más de un hilo

## Comprobación

Este cambio comprueba el almacenamiento por hilo; no cambia código de producción.

## Diseño

- **Inicialización requerida** (T16-006): un `thread_local` necesita el directorio TLS del CRT (`_tls_used`,
  `_tls_index`); con inicialización dinámica además `__dyn_tls_init` y `__dyn_tls_on_demand_init`
  (`.CRT$XDU`), que el CRT llama por hilo. Con `--crt-startup` el ejecutable lleva ese runtime. Una DLL C++
  lleva el suyo. Sin el CRT el enlace falla con esos símbolos sin definir ([auditoría](../TANDA-16-AUDITORIA-INICIAL.md)).
- **Varios hilos** (T16-007): cada hilo construye su copia la primera vez que la usa, también un hilo creado
  por la biblioteca con `std::thread` que llama a un callback NativeSharp.

## Evidencia

Fixture nuevo `pruebas-tanda-16-tls/validar.py`, con y sin `--dll`: un `thread_local` con constructor que
numera las copias con un contador atómico; el hilo principal lee la suya antes y después, y un hilo de la
biblioteca lee la suya desde un callback NativeSharp. Resultado 3030 (copias 10 y 20, dos construcciones),
igual que el control C++ de Clang 23.1.0, O0–O3.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática (`--crt-startup`) | pasa (O0–O3) | `A:/temp/nsc-t16-tls-djbbbk2m` |
| DLL (entrada NativeSharp) | pasa (O0–O3) | `A:/temp/nsc-t16-tls-ioi1j639` |

## Límites

- Un hilo creado por NativeSharp no se probó (NativeSharp no crea hilos en estas pruebas).
- Leer un `thread_local` C++ directamente desde NativeSharp sigue las reglas de la tanda 10 (`constinit`).
