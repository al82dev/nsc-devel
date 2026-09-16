# Documentación versionada de NativeSharp

Cada subdirectorio representa una línea del producto y contiene una edición autocontenida. Un
programador debe abrir la carpeta que coincida con la familia mostrada por `nsc --version`; los
sufijos `-dev` y `+build` describen el estado y la identidad concreta del ejecutable.

## Ediciones disponibles

- [NativeSharp 2.0](2.0/README.md) — edición estable vigente; compilador `2.0.0`.
- [NativeSharp 1.0.1](1.0.1/README.md) — edición N-1 y fuente de migración a 2.0.

Los expedientes bajo `mkdown/` pertenecen al repositorio de desarrollo y conservan la historia de
ingeniería. No se copian aquí porque esta carpeta contiene documentación de producto, no el diario
de implementación.

---

# Versioned NativeSharp documentation

Each subdirectory is a self-contained product-line edition. Open the directory matching the family
reported by `nsc --version`; `-dev` and `+build` identify the executable's status and exact build.
The current stable edition is [NativeSharp 2.0](2.0/README.md). NativeSharp 1.0.1 remains the
certified N-1 edition and migration source.
