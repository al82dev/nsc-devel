# Tanda universal 34: ownership de dependencias de Job

## Problema confirmado

`nsc.item373.job-dependencies-cancellation` construye un `Job<T>` con dependencias existentes que
el job observa, pero no posee. El campo `dependencies_` había sido cambiado a `owned IJob[]` pese a
que su comentario y su destrucción describen un arreglo propio con elementos no propietarios.

Con esa declaración, cada escritura intenta convertir una referencia `IJob` no propietaria en un
elemento propietario. El compilador rechaza correctamente esa conversión.

## Cambio

Se restaura `dependencies_` como `IJob[]`:

- la instancia conserva la propiedad de la reserva del arreglo y la libera explícitamente;
- los elementos continúan siendo referencias observadas;
- cancelar, esperar o consultar una dependencia no transfiere su propiedad;
- no se debilitan las reglas generales de ownership.

## Archivos

- Modificado: `lib/NativeSharp.Core.Threading/Job.ns`.
- Nuevo: este documento.

## Validación requerida

1. Ejecutar `nsc.item373.job-dependencies-cancellation` en O0 con NSLink.
2. Incluir el cambio en la siguiente reconstrucción y batería CTest completa.

## Evidencia dirigida posterior al commit

- Una primera invocación directa de CTest no exportó `NS_LIB_ROOT` y se detuvo al resolver
  `NativeSharp.Core.Threading`; no alcanzó el contrato de ownership.
- Con `NSC_LINKER`/`NSC_LLD_LINK` en `A:\nslink-devel\bin\release\nslink.exe` y
  `NS_LIB_ROOT=A:\native-sharp-3.0-baseline\lib`,
  `nsc.item373.job-dependencies-cancellation` aprobó en `0.67 s`.
- El fallo 292 permanece abierto en el inventario integral hasta la batería completa O0 con NSLink.
