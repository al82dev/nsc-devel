# Tanda universal 36: superficie acumulativa de Console

## Problema confirmado

La sincronización de `NativeSharp.Core.Console` conservó sólo cuatro overloads con `string` y
eliminó en un mismo cambio:

- cuatro overloads sin argumentos;
- 32 overloads escalares para `bool`, `char`, enteros y flotantes;
- los tres contratos de `ReadLine`.

La prueba 22 detectaba los dos primeros grupos. El tercero seguía siendo parte de la superficie
pública y del hook de runtime aunque no aparecía como una fila adicional del resumen.

## Cambio

Se restaura el bloque público completo:

- `Write`, `WriteLine`, `ErrorWrite` y `ErrorWriteLine` sin argumentos;
- las cuatro operaciones para los ocho tipos primitivos soportados;
- `ReadLine(out int)`, `ReadLine()` y `ReadLine(char[])` con sus códigos de estado.

Los overloads escalares delegan en `String.ToString`; no duplican formato. El overload primitivo de
entrada mantiene la firma exacta que el compilador sustituye por el hook de Windows.

## Archivos

- Modificado: `lib/NativeSharp.Core/Console.ns`.
- Nuevo: este documento.

## Validación requerida

1. Ejecutar `nsc.ns3.t30.console-input` en O0 con NSLink.
2. Incluir el cambio en la siguiente reconstrucción y batería CTest completa.

