# Instrucciones de trabajo para NativeSharp

## NSMETA: instrucción vigente del propietario

- Existe un único contrato vigente de NSMETA, el que soporta el compilador actual.
  Las etiquetas históricas de formato no son versiones de producto ni una guía de desarrollo.
- El lenguaje evoluciona de forma acumulativa: una capacidad ya admitida debe seguir funcionando
  cuando el compilador incorpore otras. La compatibilidad se determina por el contenido y las
  capacidades necesarias, no por comparar etiquetas históricas de NSMETA.
- No consultar expedientes históricos de versionamiento de NSMETA para planificar, modificar o
  validar trabajo actual; fueron archivados fuera del repositorio. No modificarlos ni restaurarlos.
- Esta prohibición también aplica a referencias históricas que permanezcan mezcladas en otros
  documentos del repositorio: no utilizarlas como requisitos ni como motivo para agregar versiones,
  migraciones o comprobaciones entre versiones de NSMETA. El contrato actual prevalece.
- Las pruebas existentes se conservan intactas. Esta instrucción no afirma que todas las formas
  de consumo de artefactos antiguos estén verificadas: los comportamientos se demuestran con pruebas.

Leer primero `A:\LINEAMIENTOS-DE-TRABAJO-Y-PORTEO.md` y `docs/CONTRATO-VIGENTE.md`.

- Usar exclusivamente el NativeSharp congelado de `C:\native-sharp`, resuelto por el PATH nativo.
- Existe un solo lenguaje acumulativo y un solo contrato público vigente. Las etiquetas históricas de
  compilador, lenguaje, metadata y layouts no seleccionan productos o dialectos alternativos.
- Los expedientes y versiones antiguas son evidencia histórica, no órdenes de migración o reinstalación.
- Antes de atribuir una falla al SDK, contrastar invocación y referencias con el flujo funcional del
  propietario. No imponer el consumo binario explícito como sustituto del flujo que éste ya usa.
- No reconstruir Core, cambiar metadata, recompilar el compilador, instalar ni sustituir artefactos para
  resolver un diagnóstico sin causa demostrada y autorización expresa para esa acción.
- Esta revisión documental no autoriza modificar fuentes, pruebas, bibliotecas, herramientas ni instalación.
- Los originales históricos retirados de consulta activa están en
  `A:\native-sharp-documentation-history\2026-11-06`; consultarlos sólo para investigación histórica.
