# Validacion 147 — Jerarquia principal de Historial en Builder 3

## SCOPE LOCK

- Objetivo: separar visual y estructuralmente los resultados de la corrida
  actual de las corridas guardadas.
- Naturaleza: correccion puntual de jerarquia de interfaz.
- Archivo funcional autorizado: `nscbuilder3.py`.
- Comportamiento preservado: carga, filtrado, consulta y reejecucion del
  historial; log, errores y fallos de la corrida actual.
- Protegido: motor, compilador, pruebas, manifiestos, CMake, despliegue y
  unidad `C:`.
- Validacion autorizada: sintaxis, smoke de GUI y diferencial.

## Causa

`Historial` estaba contenido dentro de `Resultados`. Esa jerarquia comunicaba
que una corrida anterior formaba parte de los resultados actuales y obligaba a
abrir las corridas historicas como tabs hermanos del log vivo.

## Implementacion

- La barra principal contiene `Construccion`, `Pruebas`, `Resultados` e
  `Historial`.
- `Resultados` contiene solamente `Log completo`, `Errores condensados` y
  `Fallos exactos` de la corrida actual.
- `Historial` contiene un panel interno `Corridas guardadas` con la lista y la
  vista previa.
- Cada corrida seleccionada se abre como otro tab dentro del panel de
  `Historial`, nunca dentro de `Resultados`.
- Dentro de cada corrida permanecen las secciones `Pruebas` y `Reporte`.

## Aseguramiento

- `python -m py_compile nscbuilder3.py`: PASS.
- `python nscbuilder3.py --gui-smoke`: PASS.
- El smoke exige la jerarquia principal exacta, confirma que `Resultados` no
  contenga `Historial` y verifica que una corrida se abra dentro del workspace
  historico.
- No se compilo ni ejecuto producto, no se ejecuto baseline, no se desplego y
  no se toco `C:`.

## Estado

Implementacion lista para inspeccion visual del usuario. La tarea persistente
continua con la clasificacion causal de la corrida funcional.
