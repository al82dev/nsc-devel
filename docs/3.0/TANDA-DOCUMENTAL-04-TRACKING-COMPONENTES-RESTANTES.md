# Tanda documental 04 — tracking de componentes restantes

Fecha: 2026-10-04

Rama: `development`

## Objetivo

Sustituir estados genéricos del catálogo NativeSharp 3.0 por estados sustentados en el código,
commits y expedientes reales de Bindings, Project Manager, Distribución, Saru y Runtime/STL.

## Cambios

- Se auditaron 58 ítems y se clasificaron como `PRUEBAS DIRIGIDAS`, `EN CURSO`, `PENDIENTE`,
  `REQUIERE AUDITORÍA` o `TERMINADA` sin confundir implementación parcial con certificación.
- Bindings registra la generación Language 3, los 33 paquetes precompilados y su consumo desde
  Saru; pointers, callbacks, errores/handles y hosts C/C++ conservan auditoría explícita.
- Project Manager registra la GUI y la orquestación ya probadas, y mantiene abiertos schema 3.0,
  workspace graph, lockfile, pack/publish y `nsc explain`.
- Distribución registra manifest SHA-256, actualización por ownership, deploy gráfico y archivo 7z
  plano, manteniendo abiertas la transacción completa, la prueba offline y la higiene integral.
- SaruGL y SaruDX quedan reconocidos como consumidores 3.0 construidos con NSLink; depuración
  cross-DLL y la demo insignia permanecen abiertas.
- Runtime/STL deja de usar `REQUIERE AUDITORÍA` en capacidades que ya fueron auditadas: las
  superficies ausentes quedan `PENDIENTE`, las parciales `EN CURSO`, y E045/E046 conservan su
  cierre documental o contractual dirigido.
- El contrato de distribución ahora refleja la política vigente: `C:\native-sharp` es 3.0 activa,
  `C:\native-sharp-2.0` es rollback, el staging nace en `A:` y C: sólo recibe por orden explícita.
- Las fuentes Saru apuntan a sus repositorios reales bajo `A:\porting-to-3.0.0`.
- El verificador rechaza futuras regresiones de esas rutas y de la política active/rollback.

## Archivos

- `tools/native_sharp_3_catalog.py`: contratos canónicos corregidos para Distribución y Saru.
- `tools/verify_native_sharp_3_catalog.py`: invariantes de rutas operativas.
- `tests/unit/test_native_sharp_3_catalog.py`: prueba dirigida de los valores canónicos.
- `A:\DOCUMENTO-MAESTRO-NATIVESHARP-3.0.md`: catálogo y resumen por componente regenerados.
- `A:\NATIVESHARP-3.0-TRACKING.md`: tabla compacta regenerada desde el maestro.

## Validación permitida

Sólo se ejecutan las pruebas Python específicas del catálogo y su verificador. No se ejecuta
baseline, CTest general, compilación de librerías, deploy ni instalación.
