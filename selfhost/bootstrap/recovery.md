# Recuperación mediante nsc0

## Objetivo

Reconstruir una generación válida sin depender de una instalación global ni destruir evidencia.

## Procedimiento

1. Detener cualquier bootstrap activo y conservar su directorio de evidencia.
2. Verificar que el checkout corresponde al commit registrado por el último gate válido.
3. Resolver rutas mediante un archivo `nscbuilder-paths` version 1.
4. Ejecutar `verify_environment.py` contra `bootstrap.lock`.
5. Rechazar la recuperación si cambia cualquier hash bloqueado.
6. Crear un directorio de salida nuevo; nunca reutilizar objetos de una generación fallida.
7. Reconstruir `nsc0` con el toolchain C++ certificado cuando el binario semilla no esté disponible.
8. Usar exclusivamente la ruta absoluta de `nsc0` para reconstruir `nsc1`.
9. Ejecutar la smoke suite y registrar un gate nuevo que enlace al último gate válido.
10. Promover el artefacto sólo después de completar el baseline correspondiente.

## Prohibiciones

- No copiar un `nsc.exe` sin registrar origen y hash.
- No usar el primer `nsc.exe` encontrado en `PATH`.
- No sobrescribir una generación válida con una generación fallida.
- No eliminar evidencia de la primera divergencia.
- No escribir en una instalación que el operador declaró fuera de alcance.
