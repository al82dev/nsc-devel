# Los usos de una llamada desconocida se evalúan antes de su propio escape

## Problema

Al compilar `X:\NativeSharp-Games-DX\al82dev` en O3, la regla conservadora de vida de arreglos
(docs/3.0/main-fixes/04 y 08) rechazaba tres llamadas de `WindowsSystemMetrics.ns`:

```
if (status >= 0 && locator != null)
{
    raii WideString namespaceSource = new WideString("ROOT\\WMI");
    namespaceName = SysAllocString(namespaceSource.Pointer());     // aceptado
    ...
}
...
raii WideString languageSource = new WideString("WQL");
queryLanguage = SysAllocString(languageSource.Pointer());       // error: cannot prove that the
                                                                //   referenced object is still alive
```

`Pointer()` no tiene resumen, así que en `recordArrayReferenceFieldCall` (`ArrayReferenceFieldEffects.cpp`)
toma el camino `UnknownAll`. Ese camino hacía, en este orden:

1. escapaba al receptor (`escapeReceiver`) y agregaba sus objetos a `routeExposed`;
2. agregaba como usos todos los arreglos de campo del receptor, incluido el centinela;
3. evaluaba esos usos.

Como el paso 1 ya había escapado los arreglos de `languageSource`, la evaluación del paso 3 tomaba como
candidatos todos los objetos expuestos. Entre ellos estaba `namespaceSource`, expuesto por su propio
`Pointer()` y destruido al cerrar su bloque (fin posible tras la unión). El receptor nuevo nunca había
recibido ese objeto, pero el uso salía `Unproven`.

El nsc instalado (`b75373e376b6e579`) rechaza lo mismo.

## Cambio

La exposición propia de la llamada (agregar `passed` y los objetos del receptor a `routeExposed` y
escapar al receptor) se mueve a `applyUnknownCallExposure`, que corre después de evaluar los usos. Los
usos de una llamada nombran lo que el receptor ya guardaba antes de ella. Lo que el código no modelado
guarde durante la llamada sí queda expuesto para todo lo que viene después, igual que antes.

El cambio es coherente con la regla del mismo bloque: una llamada desconocida es un uso de lo que el
receptor guardó, no de lo que el código desconocido pueda hacer con lo que recibe.

## Comportamiento conservado

- Un receptor que ya escapó en una llamada anterior sigue tomando como candidatos los objetos
  expuestos. El control `holder.Keep(first)` y luego `holder.Use()`, con `first` destruido entre las
  dos llamadas, se sigue rechazando.
- Los usos resumidos (`Kind::Use`) y los guardados resumidos no cambian.

## Pruebas

Nueva: `tests/integration/unknown_call_uses_before_escape.py`.

- El caso válido (dos `raii` en bloques distintos, cada uno con una llamada virtual) sale con 42 en
  O0..O3.
- El control que guarda el objeto destruido se sigue rechazando.

## Evidencia

- Prueba nueva: con este build termina con 0 y con el nsc instalado falla en el caso válido
  (`X:\NativeSharp-Games-DX-evidencia\pre-escape\integracion-*.log`).
- Existentes sin modificar: `t06.escape-summaries` `PASS: 575 checks`, `Stage58` `PASS: 71 checks`,
  `array-reference-alias-routes` `PASS: 12 checks` (`A:\nsc-devel-fixes-evidencia\unit-*-preescape.log`).
- `al82dev` compila en O3 con este build sin cambios en su código
  (`X:\NativeSharp-Games-DX-evidencia\al82dev-O3.log`).
