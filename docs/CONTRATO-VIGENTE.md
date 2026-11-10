# NativeSharp: contrato vigente de trabajo y consulta

Instrucción vigente del propietario, confirmada el 2026-11-06.

## Instalación autorizada

Se utiliza exclusivamente la instalación congelada en `C:\native-sharp`, mediante el `nsc` del PATH
nativo de Windows. Las fuentes de `A:\nsc-devel` sirven para consultar la implementación; no sustituyen
esa instalación. No se eligen compiladores de desarrollo, copias temporales ni otras instalaciones.

## Un solo lenguaje y un solo contrato

**NSMETA — decisión vigente del propietario (2026-11-08):** existe un único contrato vigente,
el soportado por el compilador actual. No se usan etiquetas históricas de NSMETA como productos
separados, como requisito para incorporar capacidades ni como criterio para rechazar contenido
que el compilador entiende. NativeSharp evoluciona acumulativamente: añadir capacidades no debe
dejar inutilizables programas anteriores que utilizan características aún soportadas.

Los expedientes históricos de versionamiento de NSMETA fueron retirados de la consulta activa y
resguardados en `X:\nsc-nsmeta-historico`; no deben utilizarse como instrucciones de desarrollo.
Las pruebas existentes se mantienen. La compatibilidad efectiva de un artefacto concreto se
verifica por su contenido y las capacidades que necesita; esta decisión no acredita por sí sola
que todos los artefactos antiguos hayan sido probados.

NativeSharp es un lenguaje acumulativo. Las etiquetas históricas de compilador, lenguaje, metadata y
layouts no seleccionan productos, dialectos o contratos públicos alternativos. No se agrega `<Language>`
a un proyecto para habilitar o deshabilitar características.

La instalación declara, mediante `nsc --print-compatibility-policy` (consulta del 2026-11-06):

```text
compiler_compatibility=capability-based;version-label-informational
language_compatibility=capability-based;version-label-informational
nsmeta_read=content-compatible;version-label-informational
```

Los números internos de formatos y layouts no son opciones de producto ni autorizan cambiar la instalación.

## Diagnóstico antes de cambiar nada

Un fallo de una invocación no demuestra que el compilador, el SDK o los juegos estén rotos. Primero se
comprueban el comando efectivo, la forma habitual de consumo, las referencias y los artefactos resueltos.
No se impone `--precompiled-only`, una referencia binaria nueva o una raíz distinta como requisito de
validación de programas que funcionan mediante otro flujo autorizado.

Un diagnóstico interno se conserva literalmente como evidencia. No se elimina su comprobación ni se
modifica metadata para ocultarlo. Tampoco se propone reconstruir Core, cambiar de compilador, reinstalar
o alterar los juegos sin demostrar la causa y obtener autorización para actuar sobre la instalación.

## Evidencia de esta revisión

Con `C:\native-sharp\bin\nsc.exe`, build `4998f7ed435b91d5`, el ejemplo `00D_pipeline_inspector`:

- pasó `nsc --check`;
- compiló por la entrada normal de archivo `.ns`, con `-O0`;
- se ejecutó y mostró `Expression: (12 * 4) + 7 = 55`.

La referencia explícita a `C:\native-sharp\lib\NativeSharpCore.nsmeta` con `--precompiled-only` produjo
un rechazo de layout. Eso sólo acredita esa ruta concreta; no una regresión general de la instalación.
No justifica sustituir el flujo funcional del propietario ni reconstruir bibliotecas.
No se recompilaron los juegos ni se certificó toda su batería en esta revisión documental.

## Historia y autoridad documental

Los directorios numerados, tandas, planes y expedientes conservan historia técnica. No establecen otra
instalación autorizada ni otro contrato público vigente. Los documentos con antiguas exigencias de ABI
numeradas se preservan en `A:\native-sharp-documentation-history\2026-11-06`; sus ubicaciones originales
contienen enlaces de archivo, no instrucciones de migración.

Los números de interfaces de herramientas independientes, como el servicio de lenguaje o el debugger,
no representan versiones alternativas de la ABI del lenguaje NativeSharp.

Este documento reemplaza, para decisiones de uso e instalación, las afirmaciones contradictorias de esos
expedientes. Los originales siguen disponibles como evidencia de su revisión histórica.
