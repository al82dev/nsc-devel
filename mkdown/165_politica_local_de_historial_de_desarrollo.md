# Política local de historial de desarrollo

## Propósito

La historia de NativeSharp debe permitir reconstruir no sólo qué cambió, sino también
por qué se eligió la implementación, qué contrato conserva y con qué compilador y
matriz se verificó. Esta política rige los cambios locales a partir de NativeSharp
1.0.1.

## Unidad de historia

Cada cambio coherente se cierra con:

1. implementación, pruebas y especificación que le correspondan;
2. un documento Markdown numerado bajo `mkdown/`;
3. una entrada en `mkdown/CONTENIDO.txt`;
4. verificación enfocada proporcional al riesgo;
5. baseline completo cuando el cambio afecta compilador, Core, linker, runner o
   contrato ejecutable;
6. un commit local con título y descripción en español e inglés.

No se mezclan defectos independientes en un solo commit sólo porque se descubrieron en
la misma corrida. Un defecto encontrado por el baseline obtiene su propio cambio; el
acta del baseline se registra después de recuperar el verde.

## Contenido obligatorio del Markdown

El documento de cada cambio debe indicar, cuando aplique:

- objetivo, caso rojo o comportamiento anterior;
- archivos creados y modificados;
- funciones, tipos, estructuras y contratos tocados;
- implementación realizada;
- razón de diseño y alternativas descartadas;
- invariantes que deben conservarse;
- pruebas añadidas o actualizadas;
- verificación enfocada y sus resultados;
- corrida completa del baseline, identidad del compilador y evidencia;
- pendientes conocidos;
- título y descripción de referencia del commit en español e inglés.

No se presenta como defecto una diferencia deliberada del diseño nativo. El documento
debe distinguir entre adaptación normal a NativeSharp, característica fuera de diseño
y fallo real del compilador o Core.

## Commits locales

El commit se crea cuando la unidad está implementada, documentada y verificada. El
título recomendado tiene esta forma:

```text
Título en español / English title
```

El cuerpo contiene al menos un párrafo `ES:` y uno `EN:`. Ambos describen el resultado
y las garantías, no una lista mecánica de archivos.

Antes de guardar el commit se revisan explícitamente los archivos preparados y se
ejecuta la validación de whitespace del diff. Se excluyen cambios ajenos, binarios,
objetos, logs, temporales y demás productos reproducibles de construcción, salvo que
un artefacto concreto sea parte versionada del contrato —por ejemplo
`_abi/gates/G00/gate.json`.

No se descartan ni sobrescriben cambios que ya estaban en el árbol sin identificar su
propietario y propósito.

## Baseline canónico

La forma automatizada es:

```powershell
python nscbuilder.py --baseline --jobs 16
```

La corrida debe usar el checkout canónico, construir lo que está en disco, instalar el
mismo `nsc.exe` que se probará y ejecutar la matriz completa con `lld-link` y NSLink en
`-O0` y `-O2`. Un verde parcial no se registra como baseline.

Para cerrar un cambio ejecutable se conserva:

- Run ID y ruta del reporte;
- conteos de CTest, fixtures y triage;
- fallos, tiempos y estado G00;
- versión/build ID del compilador instalado;
- hashes del compilador construido e instalado;
- conteo y digest S0/S1.

Si el baseline encuentra un defecto, se conserva la corrida roja, se corrige la causa
y se repite toda la matriz. El acta certificada debe enlazar ambos resultados.

Los agregados CTest que comparten `%TEMP%\nsc-cc1-rest` se ejecutan en serie. El runner
canónico ya garantiza ese orden entre enlazadores.

## Excepción de documentación

Un cambio que sólo modifica documentación o esta política no requiere repetir la
matriz binaria recién certificada. Sí requiere revisar el diff, comprobar sus enlaces
y mantener actualizado `mkdown/CONTENIDO.txt`. La excepción y el baseline vigente se
anotan en su cierre.

## Referencia de commit / Commit reference

**Título ES:** `Define la política local de historial de desarrollo`

**English title:** `Define the local development history policy`

**Descripción ES:**

> Establece una unidad de historia por cambio con documentación técnica numerada,
> commits bilingües, verificación enfocada y baseline G00 para cambios ejecutables.
> Define también la separación de artefactos generados y la evidencia mínima que debe
> conservar cada certificación.

**English description:**

> Establish one history unit per change with numbered technical documentation,
> bilingual commits, focused verification, and a G00 baseline for executable changes.
> Also define generated-artifact separation and the minimum evidence retained for each
> certification.
