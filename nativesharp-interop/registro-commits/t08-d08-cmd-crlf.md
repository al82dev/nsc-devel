# T08-D08: los `.cmd` y `.bat` versionados pasan a CRLF

Fecha: 2026-11-09. Casilla: T08-D08.

## Problema

En la validación integral, `pruebas-tanda-08-identidad-c\validar.cmd` falló con «The system cannot find the batch
label specified - setup_atomic» ([evidencia](t08-validacion-integral-20261109.md)). cmd.exe busca las etiquetas de
`goto` y `call :etiqueta` releyendo el archivo por bloques, y con finales LF puede no encontrarlas.

## Causa raíz

`.gitattributes` solo tenía `* text=auto`. El índice guarda LF, y el final de línea de la extracción depende de la
configuración de cada máquina:

- Git para Windows, con sus valores por defecto, escribe CRLF;
- con `core.eol=lf`, o con Git fuera de Windows, escribe LF.

Además, las herramientas que reescriben archivos (generadores y ediciones automáticas) habían dejado en LF 176 de
los 346 `.cmd`/`.bat` versionados. Git no lo mostraba como cambio, porque con `text=auto` normaliza al comparar.

## Cambio

- `.gitattributes` conserva `* text=auto` y añade `*.cmd text eol=crlf` y `*.bat text eol=crlf`. La extracción de
  esos archivos es CRLF con cualquier `core.autocrlf` o `core.eol`.
- El índice no cambia: sigue guardando LF, como el resto de los archivos de texto. Por eso el commit no incluye los
  `.cmd`.
- Árbol de trabajo:
  - `A:\nsc-interop-build\claude\scripts\convertir-cmd-crlf.py` convirtió a CRLF los 176 archivos versionados que
    estaban en LF, sin tocar ningún otro;
  - `refrescar-cmd.py` volvió a registrarlos en el índice con `git add --renormalize`. No cambió ningún blob, y
    `git status` quedó limpio.

## Prueba

**Nueva:** `pruebas-tanda-08-infraestructura/validar-finales-cmd.py`. Todas sus llamadas a git usan
`core.autocrlf=false` y `core.eol=lf`, la configuración que antes producía LF.

- **Repositorio:**
  - los 346 `.cmd`/`.bat` versionados tienen `eol: crlf`;
  - el árbol de trabajo es CRLF puro;
  - la extracción (`git archive --worktree-attributes`) es CRLF.
- **Control, en un repositorio temporal:** con solo `* text=auto`, la extracción de un `.cmd` y de un `.bat` es LF.
  Es el modo de fallo anterior.
- **Caso positivo, en el mismo repositorio:**
  - con las reglas nuevas, la extracción es CRLF;
  - un clon nuevo escribe CRLF y queda limpio;
  - su `.cmd`, que usa `call :etiqueta`, se ejecuta con código 0.
- **Caso negativo:** el verificador señala un `.cmd` LF y uno con finales mixtos en el árbol de trabajo.

Antes del cambio, la misma prueba fallaba en el repositorio:

- 346 archivos sin `eol=crlf`;
- 176 archivos en LF en el árbol de trabajo.

## Regresión

`A:\nsc-interop-build\claude\scripts\regresion-d08.cmd`, resultado 0:

- la suite nueva;
- `pruebas-tanda-08-identidad-c\validar.cmd`, el caso del gate que fallaba por LF. Ahora pasa completo, incluidas
  `externa-dos-bibliotecas` con la variante `atomic` (etiqueta `setup_atomic`) y las sondas.

En la primera ejecución, `identidad-c\validar.cmd` falló por otro motivo: «metadata payload has trailing bytes».
Lo daban `nsc_cpp_c_identity_tests` y `nsc_cpp_c_anonymous_identity_tests`, construidos antes del cambio de NSMeta
de T08-D02 (`pointeeQualifiers`). Se reconstruyeron de forma incremental, junto con los demás ejecutables de prueba
que usan los validadores (`A:\nsc-interop-build\claude\scripts\tests-exe.py` los lista). El compilador de esa
ejecución ya incluía el cambio de T08-D09, que se registra en su propio commit.
