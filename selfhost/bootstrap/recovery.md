# Recuperación mediante nsc0

## Objetivo

Reconstruir una generación válida sin depender de una instalación global ni destruir evidencia.

## Procedimiento

1. Detener cualquier bootstrap activo y conservar su directorio de evidencia.
2. Verificar que el checkout corresponde al commit registrado por el último gate válido.
3. Resolver explícitamente la ruta del compilador semilla y de los artefactos NativeSharp.
4. Crear un directorio de salida nuevo; nunca reutilizar objetos de una generación fallida.
5. Reconstruir `nsc0` cuando el binario semilla no esté disponible.
6. Usar exclusivamente la ruta absoluta de `nsc0` para reconstruir `nsc1`.
7. Ejecutar la smoke suite y registrar un gate nuevo que enlace al último gate válido.
8. Promover el artefacto sólo después de completar el baseline correspondiente.

Python, CMake, Ninja y el toolchain C++ son decisiones del operador para construir y ejecutar
la infraestructura de pruebas. SH53 no congela sus rutas, versiones ni hashes: certifica los
binarios NativeSharp producidos y su comportamiento observable.

## Prohibiciones

- No copiar un `nsc.exe` sin registrar origen y hash.
- No usar el primer `nsc.exe` encontrado en `PATH`.
- No sobrescribir una generación válida con una generación fallida.
- No eliminar evidencia de la primera divergencia.
- No escribir en una instalación que el operador declaró fuera de alcance.
