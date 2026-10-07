# Tanda correctiva 17: bateria externa O0

**Fecha:** 2026-10-07  
**Repositorio:** `A:\nsc-devel`  
**Rama:** `development`

## SCOPE LOCK

- Objetivo: barrer la superficie ejecutable del lenguaje con el compilador actual y comprobar
  todas las manifestaciones externas inventariadas por NSC Builder.
- Naturaleza: validacion correctiva O0; no es baseline, certificacion ni migracion.
- Comportamiento inmutable: pruebas, fixtures, proyectos, expectativas, Core y runtime.
- Compilador probado: `A:` local, hash `7011d119aa1f6db7`.
- Enlazador: `A:\nslink-devel\bin\release\nslink.exe`.
- Acciones prohibidas: escribir o instalar en `C:`, ejecutar O1/O2/O3, modificar consumidores,
  ejecutar baseline o minibaseline y conservar residuos temporales.
- Criterio de cierre: ejecutar todas las fixtures y casos de bug triage descubiertos por el
  runner en O0, registrar cada fallo y demostrar si pertenece al compilador.

## Preparacion A-local

El compilador ya construido se instalo en un prefijo temporal bajo `A:\nsc-devel`. En el mismo
prefijo se emitio `NativeSharpCore.lib` y su metadata en O0 para cerrar dependencias sin copiar
artefactos desde `C:`. Al terminar se eliminaron el prefijo, el area de trabajo y el directorio
temporal; se conservo unicamente el historial canonico de la corrida.

## Resultado

- Corrida: `A:\nsc-devel\_nscbuilder\history\20261006-235544-a5ee2c32`.
- Perfil: O0 exclusivamente.
- Casos ejecutados: 99.
- Casos correctos: 98.
- Fallos: 1.
- Tiempo: 59.791 segundos.
- Bug triage: 9 de 9 correctos.

El unico fallo fue `cc1_c23_views_and_lifetimes`: el ejecutable termino con 8 y su contrato
historico esperaba 42. No aparecio ninguna otra regresion del compilador.

La auditoria de la tanda 16 ya demostro que el LLVM contiene los tres `invoke`, handlers e
incrementos esperados, pero las fuentes protegidas actuales de `StringView` y `Span` devuelven un
valor predeterminado o realizan un no-op en lugar de lanzar fuera de rango. Ese cambio procede de
`9232bcfc0` y no pertenece a `method-bodies`/`construction`; inventar la excepcion desde NSC seria
una alteracion semantica incorrecta.

## Frontera de CTest

No se ejecuto el CTest completo en esta tanda. Aunque la GUI permita seleccionar O0, el comando
CTest no recibe ese perfil y varias pruebas historicas invocan O1/O2/O3 dentro de sus propios
scripts o ejecutables. Lanzarlo habria violado la prohibicion expresa de ejecutar esas matrices.
La corrida externa integral del usuario permanece como la autoridad para esa bateria protegida.

## Estado

- Regresiones externas atribuibles al compilador actual: ninguna observada.
- Unico fallo vigente: contrato de Core fuera del alcance autorizado.
- Pruebas, fixtures, proyectos, Core y runtime: sin modificaciones.
- Baseline y minibaseline: no ejecutados.
- O1/O2/O3: no ejecutados.
- `C:`: no modificado.
- Residuos temporales de la corrida: eliminados.
