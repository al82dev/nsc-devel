# Argumento genérico calificado por módulo en una expresión

## Problema

Con los sitios de vida de referencias ajustados, la emisión de `SaruDX11` llegaba a la generación
de metadatos y fallaba así:

```
nsc: error: Stage 48.2 static-library metadata generation failed: metadata producer read-back
verification failed: generated precompiled public template surface is not valid NativeSharp source:
SaruDX11.nsmeta.public-templates.ns:27:109: error: expected expression
    if (TextureDataTypeToken<T>.Get() == TextureDataTypeToken<SaruDX11::SaruDX.Saru.Framework.Color>.Get())
```

El fuente (`Texture2D.ns`) dice `TextureDataTypeToken<Color>.Get()`. El productor de metadatos escribe
los cuerpos de las plantillas públicas con nombres calificados por módulo (`Modulo::Ns.Tipo`). Al
releer esa superficie, el parser no la acepta:

- `::` se lexea como dos `Colon`;
- `Colon` no estaba entre los tokens que permite la vista anticipada de argumentos genéricos
  (`genericLookaheadTokenIsAllowed`, `GenericSyntaxParser.cpp`).

Por eso `looksLikeClosedGenericTypeReference` respondía que no y la expresión se leía como
comparaciones. El nsc instalado (`bef53b07d4af0012`) también rechaza esa forma en el fuente.

## Cambio

`genericLookaheadTokenIsAllowed` acepta un `Colon` solo cuando forma parte de
`Identificador : : Identificador`, el calificador de módulo que `canStartTypeAt` y el parser de tipos
ya reconocen. Un `:` de ternario sigue terminando la vista anticipada.

## Pruebas

Nueva: `tests/integration/module_qualified_generic_argument.py`. Una biblioteca con
`Token<Color>.Get()` en una plantilla pública (el caso del productor) y con la forma calificada en el
fuente se emite en O0..O3. También lleva ternarios cuyas ramas parecen argumentos genéricos
(`a < b ? a : b`, `flag ? a : b > a ? b : a`, `a < b == flag ? low : high`).

## Evidencia

En `A:\nsc-devel-fixes-evidencia`:

- Build: `compilar-nsc-calificado.log`.
- Prueba nueva: con este build termina con 0 (`calificado-nuevo.log`); con el nsc instalado falla
  (`calificado-instalado.log`).
- Existentes sin modificar: `nsc.parser` y `stage44 generics monomorphization` terminan con 0
  (`unit-parser-calificado.log`, `unit-stage44-calificado.log`).
- `SaruDX11`: con este build y los ajustes de vida de referencias en sus cuatro sitios, la emisión O0
  completa termina con `Build succeeded` (`saru-copia\emit-calificado.log`).
