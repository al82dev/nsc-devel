# Tanda correctiva 08: auditoria final

## Alcance cerrado

La serie correctiva parte del reporte externo de 234 fallos posterior a la
reingenieria semantica. La cifra no representaba 234 defectos independientes:
la mayor parte eran consecuencias de productores compartidos. El cierre del
compilador queda agrupado por causa, no por cada sintoma repetido.

| Causa | Disposicion final |
|---|---|
| Omision de `<Language>` tratada como Language 2 explicito | Corregida conservando procedencia e infiriendo solo la edicion efectiva del grafo |
| Cierre NSMETA Language 3 sobre propietario implicito | Corregido sin aceptar conflictos expresos |
| Etiquetas NSMETA usadas como sustituto de capacidades | Corregido: contenido autenticado, capacidades y productores legacy son contratos separados |
| Rutas semanticas reingenierizadas | Ocho contratos dirigidos y T37 pasan en O0 |
| Self-host optado silenciosamente a reglas Language 3 | Corregido separando edicion efectiva y edicion fuente por callable |
| Superficie Core retirada por un cambio independiente | Clasificada fuera del compilador; no se encubre alterando NSC ni pruebas |

## Auditoria de cambios

- La serie contiene ocho commits pequenos, uno por tanda logica.
- No modifica ningun archivo de prueba o fixture preexistente.
- Agrega tres pruebas nuevas e independientes para procedencia fuente, cierre
  NSMETA y semantica de una edicion omitida.
- No modifica ningun `.nsproj` historico.
- Las mejoras de rendimiento de method-bodies, construction, ownership, escape
  y scheduler permanecen incorporadas; no hubo rollback.
- No se escribio, instalo ni desplego nada en `C:`.
- No hubo `push` ni modificacion remota.
- Los archivos ajenos ya presentes en el worktree permanecen sin stage y sin
  cambios de esta serie.

## Evidencia acumulada

- Tanda 5: 6/6 contratos dirigidos de metadata/compatibilidad.
- Tanda 6: 8/8 contratos de las rutas reingenierizadas y T37 O0.
- Tanda 7: 4/4 contratos de edicion/unsafe.
- Self-host O0: emision completa de 101 fuentes y 11 unidades LLVM mediante
  NSLink.
- Imagen self-host O0: grafo de 5 proyectos y 101 unidades, module-set de 27
  unidades raiz y 4 proyectos externos, y `check-project` con 0 fallos.
- `git diff --check` limpio al cierre.

Estas comprobaciones son deliberadamente dirigidas. No equivalen a un baseline
certificado y no se presentan como tal.

## Resultado

El trabajo correctivo del compilador queda cerrado en A: con evidencia O0. La
certificacion completa queda reservada para la ejecucion externa indicada por
Alberto. No se ejecuto baseline, minibaseline ni matriz O1/O2/O3.
