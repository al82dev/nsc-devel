# Tanda universal 08: runner CTest sin reconstrucción

## SCOPE LOCK

- Objetivo autorizado: ejecutar exclusivamente los nombres incluidos en `test.run`.
- Árbol de pruebas: el ya construido en `A:\nsc-devel\build-test-nslink`.
- Compilador oficial: `C:\native-sharp\bin\nsc.exe`.
- Linker oficial: `C:\native-sharp\bin\nslink.exe`.
- Herramientas auxiliares: resolución mediante `PATH`.
- Archivos protegidos: fuentes, binarios y metadatos del árbol CTest construido.
- Prohibido: configurar CMake, compilar targets, instalar, desplegar o modificar `C:`.
- Terminación: el runner puede reutilizar el árbol existente sin alterar su contenido.

## Defecto corregido

`run_failed_tests.py` abortaba cuando no encontraba el árbol predeterminado y, después de encontrarlo,
volvía a ejecutar CMake. Eso mezclaba dos responsabilidades: preparar una construcción y repetir una selección
de pruebas ya construidas.

Además, `CTestTestfile.cmake` conserva las rutas de herramientas vigentes durante su configuración. Esa
información puede apuntar a un compilador de desarrollo aunque la repetición de fallos deba medir la instalación
oficial.

## Implementación

El runner ahora:

1. valida los dos productos NativeSharp oficiales;
2. lee el único `CTestTestfile.cmake` del árbol existente;
3. crea una copia de metadatos dentro del directorio de evidencia de la corrida;
4. sustituye en esa copia las rutas de NSC y NSLink por las instalaciones oficiales;
5. reduce las rutas de CTest y herramientas auxiliares a nombres resueltos por `PATH`;
6. ejecuta con `ctest` únicamente cada nombre enumerado en `test.run`.

La copia no contiene binarios ni fuentes. Los comandos CTest siguen apuntando a los ejecutables de prueba ya
construidos en `A:`. El árbol original no se escribe ni se reconfigura.

## Archivos modificados

- `run_failed_tests.py`: separación completa entre repetición y construcción; overlay inmutable de metadatos.
- Este documento: alcance, causa, algoritmo y evidencia de la tanda.

## Complejidad

La preparación recorre una vez el archivo CTest: tiempo `O(n)` y espacio `O(n)`, donde `n` es su tamaño. La
ejecución conserva tiempo `O(t)`, donde `t` es la cantidad de nombres únicos en `test.run`.

## Validación previa al commit

- Análisis sintáctico de Python: aprobado.
- Enumeración de `test.run`: 47 nombres únicos.
- Pruebas ejecutadas: ninguna; la política vigente exige commitear antes de ejecutar.
- Configuración, compilación, instalación y baseline: no ejecutados.
