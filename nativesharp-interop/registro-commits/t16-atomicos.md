# T16-017: operaciones atómicas compartidas entre NativeSharp y C++

## Comprobación

Este cambio comprueba la concurrencia; no cambia código de producción.

## Diseño

Un objeto atómico C++ vive en la biblioteca y se opera con sus funciones, con el modelo de memoria de C++
que Clang implementa (instrucciones con prefijo `lock` en x86-64). NativeSharp no proyecta `std::atomic` como
campo propio: lo llama. Los callbacks NativeSharp llamados por varios hilos de la biblioteca no comparten
estado NativeSharp sin sincronizar.

## Evidencia

Fixture nuevo `pruebas-tanda-16-atomicos/validar.py`, con y sin `--dll`: cuatro `std::thread` de la
biblioteca llaman mil veces cada uno a un callback NativeSharp que llama a la función de la biblioteca que
incrementa un `std::atomic<long long>` (`fetch_add`, orden secuencial) y publica un dato con liberación y adquisición. Resultado 81 (4000
incrementos y 4 publicaciones visibles), igual que el control C++ de Clang 23.1.0, O0–O3.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática (`--crt-startup`) | pasa (O0–O3) | `A:/temp/nsc-t16-atomicos-coccq754` |
| DLL (entrada NativeSharp) | pasa (O0–O3) | `A:/temp/nsc-t16-atomicos-hdy7so58` |

## Límites

- La espera atómica de la STL (`wait`/`notify`) necesita `synchronization.lib`.
- Operaciones atómicas propias de NativeSharp sobre memoria C++ no forman parte de esta tanda.
