# Argumento que el callee solo lee: no queda expuesto por ruta

## Problema

Con el `delete` genérico ya corregido, la emisión de `SaruDX11` llega a la regla conservadora de vida
de arreglos (docs/3.0/main-fixes/04 y 08) y se detiene en siete diagnósticos. Uno es
`GraphicsAdapter.ns:329`:

```
raii DisplayMode candidate = new DisplayMode(...);
if (!ContainsMode(modes, candidate))
{
    modes.Add(move(candidate));      // error: cannot prove that the referenced object is still alive
}                                    //        when this call uses a stored reference
```

`ContainsMode` es estática y solo lee `mode.Width`, `mode.Height`, etc. La frontera de llamada
(`recordArrayReferenceFieldCall`, `ArrayReferenceFieldEffects.cpp`) trata como expuesto (`routeExposed`)
todo objeto rastreado que llega a una llamada junto con otro argumento que puede guardar referencias:
supone que `candidate` pudo guardarse dentro de `modes`. En el camino donde no se agrega, `candidate`
se destruye al cerrar el ámbito. Por eso en la vuelta siguiente cualquier uso de los arreglos
desconocidos de la lista (`modes.Add`) tiene un candidato con evento de vida y se rechaza.

El nsc instalado (`bef53b07d4af0012`) da el mismo error; no lo introdujo un cambio reciente.

## Cambio

En `recordArrayReferenceFieldCall`, un argumento rastreado no se agrega a `passed` (los objetos que
la llamada puede guardar) cuando el cuerpo del callee demuestra que solo lee ese parámetro. Esto
cubre tanto a un dueño rastreado como a un local no dueño con objetos registrados. La prueba exige:

- llamada resuelta, no virtual, no override, no abstracta, no por interfaz;
- callee no precompilado, no `unsafe`, sin retorno por referencia, no async ni generador, con cuerpo
  analizado;
- parámetro por valor;
- recorrido iterativo del cuerpo sin lambdas, ensamblador en línea ni bloques `unsafe`, en el que cada
  aparición del parámetro es una lectura simple de un campo declarado (no una llamada sobre él, ni
  una referencia o `&` al campo) o un operando de `==`/`!=`.

Cualquier otra aparición (llamar un método sobre él, pasarlo a otra llamada, asignarlo, devolverlo)
conserva la exposición conservadora. El parámetro sigue escapando como receptor, igual que antes.

No se usó el resumen de escape de parámetros (`parameterEscapeSummary`). Un primer intento con
`NoEscape` aceptaba un callee que hace `holder.Slots[0] = mode`: ese resumen no cuenta un guardado en un
elemento de arreglo, así que no demuestra que el parámetro no se guarde.

## Pruebas

Nueva: `tests/integration/read_only_argument_route.py`.

- El caso válido (patrón de `GraphicsAdapter`, con un contenedor que crece) compila y sale con 42 en
  O0..O3.
- El control `Remember(holder, candidate)`, que guarda el parámetro en `holder.Slots[0]`, se sigue
  rechazando.

## Evidencia

En `A:\nsc-devel-fixes-evidencia`, build `compilar-nsc-sololectura.log`:

- Prueba nueva:
  - con este build termina con 0 (`solo-lectura-nuevo.log`);
  - con el nsc instalado falla en el caso válido con el mismo diagnóstico (`solo-lectura-instalado.log`).
- Existentes sin modificar:
  - `t06.escape-summaries`: `PASS: 575 checks`;
  - `Stage58`: `PASS: 71 checks`;
  - `array-reference-alias-routes`: `PASS: 12 checks`.

  Están en `unit-*-sololectura.log`.
- `SaruDX11`: el diagnóstico de `GraphicsAdapter.ns:329` desaparece (`saru-sololectura\emit-O0.log`).

## Lo que no cubre

Siguen seis diagnósticos de `SaruDX11` con otras causas, que no corrige este cambio:

- `AnimatedSprite2D.ns:162`. `cropped.SetData(pixels)` no tiene resumen, así que expone al receptor
  `cropped`. Después `ownedFrames.Add(move(cropped))` es un move a un parámetro owned de efecto
  desconocido (`releaseUnproven`). Sin la exposición, esa transferencia no dejaba a `cropped` como
  candidato de la lectura `ownedFrames[...]`.
- `GameMapLoader2D.ns:78`, `ModelX.ns:333` y `RuntimeModelImporter.ns:158` (dos diagnósticos):
  pendientes de aislar.
