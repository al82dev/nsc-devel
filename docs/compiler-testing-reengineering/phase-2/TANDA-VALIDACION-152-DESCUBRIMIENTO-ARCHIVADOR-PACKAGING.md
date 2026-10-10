# Tanda de validación 152 — Descubrimiento del archivador de Packaging

## Estado

`IMPLEMENTADA; VALIDACIÓN INTEGRAL DIFERIDA HASTA VALIDACIÓN 155`

## Defecto concreto

Doce variantes de Packaging terminaban como error de infraestructura porque
Builder 3 sólo buscaba `7z` y `7zz` en `PATH`. La máquina posee una instalación
normal de 7-Zip fuera de `PATH`, por lo que la suite no alcanzaba sus contratos
de empaquetado.

## Causa

El descubrimiento omitía las raíces de instalación publicadas por Windows. No
era un defecto del gate ni de los paquetes: el archivador nunca llegaba a su
entrada.

## Corrección mínima

`discover_archive_executable` conserva este orden de autoridad:

1. ruta explícita válida guardada por el usuario;
2. `7z` o `7zz` encontrado mediante `PATH`;
3. `7-Zip/7z.exe` bajo `ProgramFiles`, `ProgramW6432` o
   `ProgramFiles(x86)`.

No se fija una letra de unidad, no se cambia `PATH`, no se descarga una
herramienta y no se acepta un directorio como ejecutable.

## Evidencia preparada

Las pruebas dirigidas construyen una raíz `Program Files` temporal, demuestran
el descubrimiento sin una unidad codificada y comprueban que una ruta explícita
existente conserva prioridad.

Por instrucción expresa no se ejecutó todavía ninguna prueba. La validación
ocurrirá en la corrida única final `Debug/O0/Todos`.
