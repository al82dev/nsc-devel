# Tanda NSC 3: progreso sin fuentes virtuales de NSMETA

## Problema

Al consumir bibliotecas precompiladas, el progreso de `syntax-lex` y
`syntax-parse` mostraba rutas como:

```text
C:/native-sharp/lib/NativeSharp.Bindings/bindings/assimp/NativeSharpAssimp.nsmeta.public-api.ns
```

Esos archivos no existen en la instalación. Son nombres virtuales usados por
el compilador para el texto reconstruido en memoria desde el archivo binario
`.nsmeta`. Exponerlos en la consola hacía parecer que el consumidor estaba
compilando código fuente distribuido con las bibliotecas y también inflaba el
total visible de unidades.

## Cambio

`src/Frontend/ProjectCompilation.cpp` ahora identifica las dos clases de
fuente virtual respaldadas por metadatos:

- `.nsmeta.public-api.ns`
- `.nsmeta.public-templates.ns`

Ambas continúan lexándose y analizándose internamente. Únicamente se omiten de
los eventos públicos `syntax-lex` y `syntax-parse`. Los contadores de esas
fases incluyen sólo archivos `.ns` reales del proyecto consumidor.

## Archivos modificados

- `src/Frontend/ProjectCompilation.cpp`
- `docs/compiler-optimization/TANDA-NSC3-PROGRESO-SIN-FUENTES-VIRTUALES-NSMETA.md`

No se creó ni modificó código fuente de bindings o bibliotecas.

## Validación dirigida

- Construcción incremental exclusiva del objetivo `nsc`: correcta.
- Contrato de runtime estático de `nsc.exe`: correcto.
- Compilación O0 de `al82dev.nsproj` con 15 bibliotecas y NSLink: correcta.
- `syntax-lex`: 7/7 fuentes reales; ninguna ruta `.nsmeta.public-*`.
- `syntax-parse`: 7/7 fuentes reales; ninguna ruta `.nsmeta.public-*`.
- No se ejecutó baseline.

## Mejora observable

La consola muestra solamente el trabajo fuente que pertenece al proyecto del
usuario. Los metadatos precompilados siguen validándose sin presentarse como
si fueran archivos fuente instalados o recompilados.
