# Tanda de validación 160 — dependencia Collections de SelfHost.Types

## Evidencia dirigida

La corrida dirigida `20261010-142516-2ae342a4` cerró seis de los siete casos
seleccionados. El único residuo, `TSH-00981`, falló al construir
`SelfHostTypes.nsproj`: `TypeSystem.ns` y su módulo de pruebas usan
`NativeSharp.Core.Collections.Generic.List<T>`, pero el proyecto declaraba
únicamente `NativeSharp.Core`.

El diagnóstico `type 'List' does not name a unique visible generic class/struct
template` corresponde a una dependencia ausente de la fixture, no a un
resultado funcional del compilador.

## Corrección

`SelfHostTypes.nsproj` declara ahora su dependencia directa sobre
`NativeSharp.Core.Collections.Generic`. No se modifica el compilador, ningún
oráculo ni la expectativa de `TSH-00981`.

## Revalidación

Se ejecutó únicamente `TSH-00981` después del commit, sin repetir la batería
completa ni los otros seis casos ya cerrados:

- corrida: `20261010-142944-tsh00981`;
- configuración: `Debug/O0`;
- resultado: `1/1 PASS`;
- evidencia: `_nscbuilder3/history/20261010-142944-tsh00981/results.json`.
