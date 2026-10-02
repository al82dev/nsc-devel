# Tanda baseline 06G: contratos place/value y orden SH41

## Objetivo

Cerrar conjuntamente los fallos SH30, SH31, SH48, SH49 y SH50 observados en
la corrida `20261001-213927-392a28e7`, y auditar el resto de la infraestructura
self-host para evitar otra corrección fragmentaria.

## Causa raíz

- SH30 y SH31 conservaban seis fragmentos semánticos 2.0 con las categorías
  `lvalue/rvalue`; el dumper y los demás contratos ya publican `place/value`.
- SH47 fue retirado de la configuración ordinaria porque NSAsm es externo,
  pero SH48 todavía dependía exclusivamente del nombre opcional de SH47.
  Cuando SH47 no se registraba, CTest consideraba libre a SH48 y lo iniciaba
  antes de SH41. SH48 no encontraba `kernel32.lib`; SH49 y SH50 tampoco
  encontraban `nsc2-o0-nslink.exe`.

## Cambios

- El contrato de object model usa `place` para `this`, `base` y acceso a
  miembro.
- El contrato de texto nativo usa `value` para literales de texto/carácter y
  `place` para indexación.
- SH48 depende siempre de SH41 y SH46 (`interop-parity`).
- Cuando NSAsm se proporciona explícitamente, SH47 se agrega como dependencia
  adicional de SH48; cuando no existe, la cadena permanece completa sin un
  nombre de prueba inexistente.
- SH49 depende de SH48 y SH50 depende de SH49 como antes.

## Auditoría completa realizada

- No quedan fragmentos ejecutables `lvalue/rvalue` en
  `selfhost/contracts` ni `selfhost/bootstrap`; sólo permanece una mención
  descriptiva en el README histórico.
- Los 57 CTests `nsc.selfhost.*` fueron enumerados desde el modelo JSON de
  CTest: ninguna propiedad `DEPENDS` apunta a una prueba ausente.
- La cadena comprobada quedó:
  `SH41 -> SH42/SH43/SH44 -> SH45 -> SH46 -> SH48 -> SH49 -> SH50 -> SH51 -> SH52 -> SH53 -> SH54`.
- Las identidades `nsc-next 2.0.0-bootstrap.sh14` restantes pertenecen al
  compilador autoalojado bootstrap y no al host 3.0; son intencionales.

## T37

La corrida registró una salida transitoria 0. El mismo ejecutable generado por
esa corrida devolvió 42 en veinte ejecuciones consecutivas. Después, el CTest
T37 completo volvió a compilar, enlazar, ejecutar e inspeccionar CodeView/PDB y
pasó 1/1. No se añadió un retry que pudiera ocultar un defecto real.

## Archivos modificados

- `CMakeLists.txt`
- `selfhost/contracts/object-model.contract.json`
- `selfhost/contracts/native-text.contract.json`

## Archivos nuevos

- Este documento de trazabilidad.

## Validación dirigida

- `nsc.selfhost.object-model`: correcto.
- `nsc.selfhost.native-text`: correcto.
- Grafo CTest regenerado: SH48 depende de SH41 y SH46; SH49 depende de SH48;
  SH50 depende de SH49; cero dependencias ausentes.
- Ejecutable T37 de la corrida: 20/20 ejecuciones con salida 42.
- `nsc.ns3.t37-debug-values`: correcto 1/1.

No se ejecutó el baseline completo ni se modificó C:.
