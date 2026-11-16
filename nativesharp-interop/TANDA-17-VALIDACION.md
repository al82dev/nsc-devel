# Validación de la tanda 17 en Windows x64

Las pruebas usan Clang 23.1.0 disponible por nombre, las herramientas LLVM del mismo entorno (`llvm-pdbutil`
para los PDB), un compilador NSC de desarrollo y, en la medición de la caché, el runtime estático de MSVC con
la STL de `A:/vc2026-runtime`. Cada guion crea sus artefactos y registros en un directorio propio bajo
`A:/temp` y no modifica PATH.

Ejecutar `validar.py` en cada directorio, con `--nsc` (compilador de desarrollo) y `--clang-root` (raíz de
Clang), y otra vez con `--dll`:

| Fixture | Capacidad |
|---|---|
| `pruebas-tanda-17-diagnosticos` | importación, uso, sobrecargas, plantillas, ausencias y símbolos (T17-001 a 006) |
| `pruebas-tanda-17-depuracion` | información de depuración entre NativeSharp y C++ (T17-007, 008) |
| `pruebas-tanda-17-cache` | reutilización de la compilación y medición (T17-009 a 011) |

La guía de T17-012 es [GUIA-BIBLIOTECAS-C-CPP.md](GUIA-BIBLIOTECAS-C-CPP.md).

`pruebas-tanda-17-auditoria/sonda.py` es la sonda de la [auditoría inicial](TANDA-17-AUDITORIA-INICIAL.md);
no es una prueba de soporte.

La regresión agrupada ejecuta todos los guiones de las tandas 17 a 9 sin `--dll` y los controles CTest; sus
resultados y el cierre están en [TANDA-17-VALIDACION-AGRUPADA.md](TANDA-17-VALIDACION-AGRUPADA.md). Los
pendientes de la tanda están en [registro-commits/t17-cierre.md](registro-commits/t17-cierre.md).
