# Checkpoint de pausa: FnOnce y vistas

Nota de integración: se conserva aquí únicamente este documento de estado del
commit `88978ca12`. El código experimental y sus pruebas no se incorporaron a
la rama integradora; sus resultados corresponden a la copia de vistas.

Este checkpoint conserva trabajo en curso para revisión. **No es una solución
completa ni está listo para integrar como corrección de FnOnce.** El árbol base
antes de este checkpoint era `e9597b2f1b999f5568bf627475858067a2990b6a`.

El borrador contiene seguimiento de ambientes FnOnce y un pase experimental.
Ha mostrado falsos rechazos de programas válidos en Writer, ciclos y condiciones
correlacionadas, además de rutas inseguras aceptadas en análisis, entre ellas
creación de objeto con reentrada, llamadas Identity con Core binario, ciertos
retornos/condicionales y destinos ref. Los programas negativos no se ejecutaron.
Los helpers de dominio relacional y regiones son prototipos aislados: no
certifican el flujo real, el manejo de cleanup ni la seguridad del compilador.

El compilador restaurado de la copia aislada se construyó incrementalmente y
el control focal reprodujo el estado previo (`restored-focal-v5.log` bajo
`A:/nsc-vistas-evidencia/fnonce-identity-extraction/`). Su SHA-256 registrado
es `7993b592c7b37a1719154ea0cccd0d1ae786cd3065ac704645b5ef027247083c`.
Ese control no sustituye las regresiones completas del alcance. El código y las
pruebas en este checkpoint pueden requerir cambios antes de integrarse.

La extracción alternativa Identity solo existe en evidencia como
`identity-v5.patch` en el mismo directorio. Su SHA-256 es
`035c86c31f74ed09ee60c73697bf7dabee02d89a56759008f825ea4b56ba30bd`.
Pasó siete controles directos sin Core, pero no resuelve Core, condicionales,
`ref`, `Touch` ni los límites de 129 intentos y 257 partes. **No está incluida
como corrección en este checkpoint.** Consultar `PLAN.md`, `RESULTADO.md` y
`PAUSA.md` en ese directorio para comandos, hashes y restauración.

Los archivos de pruebas incluidos son reproducciones o probes de investigación;
algunos fallan intencionalmente frente al estado actual y no constituyen una
batería aprobada. Las evidencias y artefactos en `A:/nsc-vistas-evidencia/`
no forman parte de Git: deben conservarse y compartirse aparte si otra máquina
necesita reproducir el detalle.

El caso de reasignación condicional de `Borrow()` se describe en
`vistas-condicional-receptor-repetido.md`. No reprodujo el rechazo con el
contexto reconstruido, pero la fuente histórica íntegra no estaba disponible.

Al reanudar, contrastar los casos abiertos y separar cambios coherentes antes de
integrar. En particular, el pase experimental no convierte una ruta no modelada
en una barrera de seguridad: la ausencia de diagnóstico no acredita seguridad.
Los archivos compartidos de frontend pueden entrar en conflicto con la rama de
interoperabilidad; el integrador debe revisarlos. Mantener compilaciones
incrementales y pruebas dirigidas, sin modificar interoperabilidad C/C++, C:,
otros clones ni herramientas externas. No hacer merge, push ni despliegue.
