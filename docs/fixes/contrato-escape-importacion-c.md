# Contrato de escape de un puente hacia una importación C

## Problema

Con el nsc instalado (`C:\native-sharp\bin\nsc.exe`, build `a87f5e9ada6d1f6d`), la instalación
de bindings se detenía en `Win32.System`:

```
SystemMemory.ns:19:47: error[NS3020]: argument 1 of 'GlobalMemoryStatusEx' refers to
current-frame storage, but parameter 1 has an unknown escape contract
```

`nsb.py` (`bridge_extern_c_source`) convierte cada importación C del paquete proveedor
(`Win32.Providers.System`) en un método NativeSharp público que sólo le reenvía:

```
public static int GlobalMemoryStatusEx(MEMORYSTATUSEX* p0)
{
    unsafe { return __NsbNative_….GlobalMemoryStatusEx(p0); }
}
```

El consumidor llama ese puente con `&status`. Las reglas de `unknown-native-escape.md` y
`unknown-native-forwarded-escape.md` (commits `295f19e94` y `ed9a7f90d`) excluyen
explícitamente las importaciones C: pasar la dirección de un local a una importación directa se
acepta. Pero el parámetro de una importación empezaba como `Unknown`, así que el puente heredaba
`Unknown` y lo publicaba en NSMETA. El consumidor ve entonces un método NativeSharp precompilado
con contrato desconocido y rechaza. La misma llamada se acepta directa y se rechaza a través del
puente. El mismo patrón está en `Window.ns` (siete llamadas), y lo tendría cualquier aplicación que
pase un struct local a una API Win32 a través de estos paquetes.

## Cambio

- `language::EscapeClass` y `interop::LibraryMetadataEscapeClass` agregan `ForeignImport` (valor
  7, al final; los valores existentes no cambian). Significa que la dirección sólo llega a
  funciones C importadas, la misma confianza que ya recibe una llamada directa.
- `initializeParameterEscapeSummaries`: los parámetros de una importación C (`extern C from`, o un
  `extern` sin cuerpo que no se exporta) empiezan como `ForeignImport` en lugar de `Unknown`.
- `EscapeAnalyzer.cpp`: la unión de resúmenes pasa a `escapeClassJoin`, con rangos duplicados
  para que `ForeignImport` quede entre `CallerOwned` y `Global` sin reordenar el resto. Gana el
  rango mayor y un rango igual conserva la clase actual, como antes.
  `ForeignImport ⊔ Global = Unknown`, que es el resultado que ya daba `Unknown ⊔ Global`. Así una
  dirección que además se retiene se sigue rechazando.
- Decodificadores: NSMETA (`LibraryMetadataPayload.cpp`) y snapshots (`SnapshotCodecSupport.cpp`)
  aceptan el valor 7.
- `ClassReceiverEffectGraph.h`: `ForeignImport` cuenta como `Unknown`, como contaba antes la
  importación C.

Un puente que sólo reenvía publica `ForeignImport`. El consumidor no lo marca como contrato
desconocido (ni en `validateEscapeCall` ni en la propagación de `parameterUnknownNativeEscape_`),
y un envoltorio del consumidor que reenvía al puente también queda `ForeignImport`.

## Comportamiento conservado

- La llamada directa a una importación C sigue aceptada; para ella no cambia ninguna comprobación
  (`ForeignImport` no es `Global` ni `Unknown`, igual que antes no se marcaba).
- Un parámetro que se retiene (`Global`) y además va a C sigue siendo `Unknown`.
- Los contratos `Unknown` de métodos NativeSharp precompilados siguen rechazando direcciones del
  marco.
- La metadata producida antes de este cambio publica `Unknown` para los puentes. Hay que
  reconstruir los paquetes de bindings con este compilador para que publiquen `ForeignImport`.
- Un compilador anterior rechaza el valor 7 al leer metadata nueva (el decodificador falla
  cerrado).

## Límites

`ForeignImport` hereda la confianza que la regla ya daba a las importaciones C: no demuestra que
la función C no retenga el puntero. Es la misma hipótesis de las llamadas directas.

## Pruebas

- Nueva: `tests/integration/foreign_import_bridge_escape.py`.
- Existentes, sin modificar: ver Evidencia.

## Evidencia

En `A:\nsc-devel-fixes-evidencia`, build `compilar-nsc-foreign2.log` (sin errores ni warnings):

- Reproducción mínima (`bindings-ns3020\`, puente con la forma de `nsb`): con el nsc instalado,
  el consumidor falla con NS3020 directo y a través de una fachada (`sonda-puente-instalado.log`);
  con este build compila (`sonda-puente-nuevo.log`).
- Prueba nueva `foreign_import_bridge_escape.py`: con este build, los dos casos válidos compilan en
  O0, O1, O2 y O3, el puente que retiene se rechaza y la importación directa se acepta
  (`foranea-nuevo.log`). Con el nsc instalado falla en el primer caso (`foranea-instalado.log`).
- Bindings reales: `nsb.py install --prefix A:\nsc-devel-fixes-evidencia\nsb-prefix` para
  `Win32.System` y `Win32.Windows` construyó e instaló los 10 paquetes Win32 de la cadena
  (`nsb-win32-nuevo2.log`). El prefijo está en A:; de `C:\native-sharp` sólo se leyó el paquete
  oficial `NativeSharp.Core` (O0). Para el hash de identidad que calcula `nsb`, se copió
  `nslink.exe` del PATH junto al nsc aislado.
- Pruebas existentes sin modificar: `nsc.language.t06.escape-summaries` `PASS: 575 checks`
  (`unit-t06-escape-foreign.log`); `Stage58ViewLifetimesTests` `PASS: 71 checks`; las 15 pruebas
  de la regresión dirigida de vistas y préstamos terminan con 0 (`regresion-foranea\`).
- Bibliotecas: 31 de 31 paquetes en O0 y O2 (`libs-paso3-O0.log`, `libs-paso3-O2.log`).

No verificado: la batería completa de ctest; la instalación de los demás bindings (DirectX,
ImGui, etc.) con este build; ejecución de los programas en las sondas.
