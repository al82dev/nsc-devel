# NativeSharp 2.0 self-hosting — tanda XX: nombre

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `OPEN` |
| Tareas | `NS2-SH-xxx` |
| Gate | `SHxx` |
| Rama | `development` |
| Milestone | `nativesharp-2.0-selfhost-milestone` |
| Commit de entrada | pendiente |
| Commit de cierre | este commit (`selfhost(tanda-XX): ...`) |
| Fecha de apertura | pendiente |
| Fecha de cierre | pendiente |
| Baseline de cierre | pendiente |

## 2. Objetivo y alcance

### Objetivo

Describir el resultado observable que entregará la tanda.

### Incluido

- Pendiente.

### No incluido

- Pendiente.

## 3. Diseño

### Arquitectura

Describir componentes, límites, dependencias y flujo de datos.

### Contratos e invariantes

- Pendiente.

### Decisiones y alternativas

| Decisión | Alternativas consideradas | Razón |
|---|---|---|
| Pendiente | Pendiente | Pendiente |

## 4. Inventario de archivos

| Archivo | Acción | Responsabilidad | Razón |
|---|---|---|---|
| Pendiente | crear/modificar/eliminar | Pendiente | Pendiente |

## 5. Tipos y datos

| Tipo | Clase | Responsabilidad | Invariantes | Ownership/vida |
|---|---|---|---|---|
| Pendiente | class/struct/interface/enum/alias | Pendiente | Pendiente | Pendiente |

## 6. Métodos y funciones

| Firma | Responsabilidad | Entradas y precondiciones | Salida y postcondiciones | Errores/diagnósticos | Efectos, ownership y concurrencia |
|---|---|---|---|---|---|
| Pendiente | Pendiente | Pendiente | Pendiente | Pendiente | Pendiente |

## 7. Superficie observable

Documentar opciones de CLI, variables de entorno, formatos, códigos de salida, diagnósticos,
versiones de contrato y compatibilidad afectados por la tanda.

## 8. Implementación

Describir el orden real de implementación y cualquier desviación justificada respecto del diseño.

## 9. Pruebas

| Prueba | Tipo | Propósito | Resultado | Evidencia |
|---|---|---|---|---|
| Pendiente | positiva/negativa/regresión/diferencial | Pendiente | pendiente | pendiente |

## 10. Comparación diferencial

Registrar diferencias contra `nsc0` o la generación anterior, normalizaciones autorizadas y la
resolución de cada divergencia. Escribir `No aplica` únicamente con una justificación concreta.

## 11. Baseline de cierre

### Identidad

| Campo | Valor |
|---|---|
| Comando exacto | `python nscbuilder.py --baseline --jobs N` |
| Identificador de corrida | pendiente |
| Revisión probada | pendiente |
| Fecha y entorno | pendiente |
| Reporte Markdown | pendiente |
| Reporte JSON | pendiente |
| Hash del reporte | pendiente |

### Matriz y resultados

| Matriz | Total | Aprobadas | Fallidas | Omitidas |
|---|---:|---:|---:|---:|
| CTest/lld-link | 0 | 0 | 0 | 0 |
| CTest/NSLink | 0 | 0 | 0 | 0 |
| Fixtures | 0 | 0 | 0 | 0 |
| Bug triage | 0 | 0 | 0 | 0 |
| Self-hosting acumulado | 0 | 0 | 0 | 0 |

### Corridas fallidas anteriores

Documentar cada intento fallido, primer error, causa, corrección y nueva corrida completa.

## 12. Riesgos, limitaciones y deuda

- Pendiente.

## 13. Contenido del commit

### Archivos incluidos

Pegar o enlazar el inventario verificado del commit.

### Higiene

- [ ] No contiene `.obj`, `.exe`, `.dll`, `.lib`, PDB, temporales ni cachés no normativos.
- [ ] No contiene cambios locales ajenos a la tanda.
- [ ] El manifiesto de fuentes está actualizado cuando corresponde.
- [ ] La documentación describe las firmas y archivos realmente incluidos.
- [ ] El baseline se ejecutó después del último cambio funcional.

### Commit de cierre

```text
selfhost(tanda-XX): descripcion breve
```

## 14. Criterio de cierre

- [ ] Todas las tareas declaradas están terminadas.
- [ ] Las pruebas específicas están verdes.
- [ ] El baseline completo está verde.
- [ ] El gate correspondiente tiene evidencia y problemas vacíos.
- [ ] El expediente coincide con el código del commit.
