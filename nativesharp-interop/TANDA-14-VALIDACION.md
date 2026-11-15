# Validación de la tanda 14 en Windows x64

Las pruebas usan Clang 23.1.0 disponible por nombre, las herramientas LLVM del mismo entorno y un
compilador NSC de desarrollo. Cada guion crea sus artefactos y registros en un directorio propio bajo
`A:/temp` y no modifica PATH. El entorno selecciona NSLink (`NSC_LINKER=nslink.exe`) y la biblioteca
runtime de la construcción (`NSC_WINDOWS_RUNTIME_LIBRARY`).

Ejecutar `validar.py` en cada directorio, con `--nsc` (compilador de desarrollo) y `--clang-root`
(raíz de Clang), y otra vez con `--dll`:

| Fixture | Capacidad |
|---|---|
| `pruebas-tanda-14-macros` | macros C y C++ (T14-001, 002, 003) |
| `pruebas-tanda-14-configuracion` | condiciones del preprocesador, identidad de la sesión y dependencias con caché (T14-004, 005, 011) |
| `pruebas-tanda-14-registros` | bit-fields y atributos de layout en records C++ (T14-006, 008) |
| `pruebas-tanda-14-especiales` | punteros a miembro, header precompilado y módulos C++ (T14-007, 009, 010) |

Cada fixture compara un control C++ compilado por Clang con el consumidor NativeSharp de O0 a O3; los de
macros y registros exigen además que cada símbolo C++ que NSC llama tenga los tipos físicos de Clang. Los
de macros y especiales incluyen controles negativos con su contraparte.

`pruebas-tanda-14-auditoria/sonda.py` es la sonda de la [auditoría inicial](TANDA-14-AUDITORIA-INICIAL.md);
no es una prueba de soporte.

La regresión agrupada ejecuta todos los guiones de las tandas 14 a 9 sin `--dll` y los controles CTest;
sus resultados y el cierre están en [TANDA-14-VALIDACION-AGRUPADA.md](TANDA-14-VALIDACION-AGRUPADA.md).
Los pendientes de la tanda están en [registro-commits/t14-cierre.md](registro-commits/t14-cierre.md).
