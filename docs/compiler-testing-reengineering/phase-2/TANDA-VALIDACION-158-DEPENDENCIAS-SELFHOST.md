# Tanda de validación 158 — dependencias de productores self-host

## Evidencia dirigida

La corrida `20261010-142040-70b38c9c` ejecutó únicamente los siete casos que
cubren las correcciones de infraestructura:

- `TB-BAS-021` y `TB-BAS-022`: `PASS`;
- `TSH-00001`: `PASS`;
- `TSH-00188`, `TSH-00399`, `TSH-00849` y `TSH-00981`:
  `INFRASTRUCTURE_ERROR`.

Los cuatro errores se redujeron a dos causas exactas.

## Causa 1 — raíz de referencias de biblioteca ausente

Diagnostics, Lexer y Source incluyen proyectos de biblioteca que consumen
`NativeSharpCore.lib` y `NativeSharpCore.nsmeta`. El prefijo configurado ya
contiene ambos artefactos, pero el comando que construía el fixture no entregaba
su directorio al NSC seleccionado.

La construcción self-host recibe ahora `--library-reference-root` con
`<install_prefix>/lib`. No se copia ninguna biblioteca al árbol fuente, no se
usa `C:` y no se cambia ningún proyecto de producto.

## Causa 2 — identificadores que colisionaban con palabras reservadas

`SelfHostTypesInterningModule.ns` usaba `owned` y `borrowed` como nombres de
variables locales. El fixture conserva exactamente los descriptores que prueba,
pero los identificadores locales pasan a `ownedTypeId` y `borrowedTypeId`.

## Límites

- Código de producto NSC modificado: no.
- Contratos u oráculos funcionales modificados: no.
- Casos retirados: no.
- Unidad `C:` modificada: no.
- Revalidación: exclusivamente los siete casos dirigidos, después del commit.
