# Baseline 2026-10-04: NSMETA histórico y resúmenes de escape

## Problema

Desde NSMETA 1.35 cada callable puede publicar un resumen interprocedural de escape. El escritor
rechazaba cualquier solicitud de una versión anterior si el modelo moderno contenía ese campo,
impidiendo producir deliberadamente artefactos 1.12, 1.13 y 1.16.

## Cambio

Al escribir NSMETA menor que 1.35 se eliminan los resúmenes de escape de miembros y funciones de
namespace. Los lectores antiguos ya interpretan su ausencia de forma conservadora, por lo que no
se concede una optimización ni se debilita la seguridad.

## Archivo

- `src/Interop/LibraryMetadata.cpp`

## Mejora

Se restaura la ventana histórica de escritura utilizada por las pruebas de inline cross-library,
volatile MMIO y promoción de miembros anónimos, manteniendo rechazo para características cuya
representación antigua sí sería incorrecta.
