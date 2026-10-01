# Tanda correctiva G00 — reutilización del cierre binario explícito

Fecha: 2026-10-01
Estado: terminada, pendiente de baseline completo

## Fallo observado

Un consumidor podía declarar directamente Core, Diagnostics y un productor. La NSMETA del
productor registraba a su vez Diagnostics como dependencia binaria. Aunque Diagnostics ya estaba
cargado y su fingerprint coincidía, el expansor del cierre exigía encontrar otra copia física junto
a la NSMETA del productor o bajo la raíz global. `nsc.debug.extreme-o0` fallaba antes de compilar el
consumidor.

## Regla implementada

- Antes de buscar un artefacto transitivo, el cierre consulta los módulos explícitamente cargados.
- La identidad se valida por nombre canónico de módulo y fingerprint ABI congelado.
- Si coincide, se reutiliza el artefacto explícito sin exigir una copia duplicada.
- Si el fingerprint difiere, la compilación falla antes del linker con un diagnóstico causal.
- La búsqueda física junto a la NSMETA o en raíces configuradas se conserva para dependencias que
  todavía no están cargadas.

## Archivos modificados

- `src/Driver/Driver.cpp`: reutiliza y valida una dependencia explícita antes de expandir el cierre.

## Mejora obtenida

Los paquetes pueden vivir en directorios hermanos y ser declarados directamente por el consumidor
sin tener que duplicarse dentro de cada productor. El cierre continúa siendo determinista y no
acepta sustituciones ABI incompatibles.

## Validación específica

- Build exclusivo de `nsc`: correcto.
- El fixture extreme-debug superó la carga del cierre que antes fallaba y avanzó hasta la
  verificación independiente de nombre RSDS.
- El consumidor extreme se compiló aparte con O0 y `-g0`, enlazó tres bibliotecas y terminó con
  exit 42.
- El PE temporal de esa comprobación fue eliminado.
- No se ejecutó CTest ni baseline.
