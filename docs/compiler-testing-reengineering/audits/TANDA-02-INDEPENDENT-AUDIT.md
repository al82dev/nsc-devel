# Auditoría independiente — Tanda 02

## Segunda fuente de conteo

La auditoría contrapone:

1. archivos devueltos por `git ls-files`;
2. fuentes extraídas de los targets CMake;
3. globs y referencias declarados por cada `.nsproj`;
4. includes reales encontrados en todos los `.cpp` y `.h` versionados.

## Resultado

- Host: 242 fuentes esperadas, 242 asociadas, cero faltantes y cero extras.
- NativeSharp: 119 fuentes esperadas, 119 asociadas, cero faltantes.
- Includes: 523 archivos leídos, cero ocurrencias internas sin componente.
- Targets: once productos CMake más el proyecto self-host; el generador de identidad está contado como
  productor independiente.
- Artefactos: veintiún productos o árboles instalables clasificados por productor y consumidor.

## Hallazgo de auditoría

La primera agregación por carpeta no era suficiente para representar ownership físico: dos archivos de
`Driver` pertenecen al target `nsc_compilation`. Se conservaron ambos ejes en lugar de forzar una sola jerarquía.
Este hallazgo no modificó código ni build; corrigió el modelo documental antes de diseñar pruebas.

## Estado

`LISTA` para la Tanda 02. No implica que los 57 componentes hayan sido leídos semánticamente ni que sean
reconstruibles.
