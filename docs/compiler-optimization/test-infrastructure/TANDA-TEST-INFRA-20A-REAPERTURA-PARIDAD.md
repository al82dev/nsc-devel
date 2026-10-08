# Tanda 20A — Reapertura y contrato de paridad de NSC Builder 2

## Motivo

La primera implementación de Builder 2 cubrió y verificó la infraestructura
consolidada O0, pero fue cerrada incorrectamente como si eso demostrara paridad
con el builder anterior. Una restricción de ejecución no autoriza reducir la
implementación.

La tanda 20 vuelve a `EN CURSO`. El código existente se conserva como base
válida, pero ya no se presenta como reemplazo completo.

## Arquitectura obligatoria

Builder 2 tendrá fronteras separadas y ordenadas:

1. **Compiler Build**: configura y construye NSC en Release o Debug.
2. **Compiler Install**: instala únicamente un build que posea recibo válido.
3. **Test Configuration**: sólo se habilita después de validar el recibo y el
   hash del compilador construido.
4. **Test Execution**: selecciona O0, O1, O2, O3 o todos; caso, categoría o
   batería; registro unitario y suites especializadas.
5. **Evidence**: salida completa, errores causales, resultados, historial,
   reintentos y limpieza permanecen separados de la configuración.
6. **Toolchain**: construcción independiente de NSLink, NSASM y NSDBG;
   staging en A: junto al NSC construido y selección explícita de herramientas
   incluidas en deploy.

## Matriz de paridad

| ID | Capacidad del builder anterior | Estado Builder 2 |
|---|---|---|
| B2-001 | Construcción del compilador separada de pruebas | PARCIAL; requiere recibo formal |
| B2-002 | Configuración Release | PENDIENTE |
| B2-003 | Configuración Debug | PENDIENTE |
| B2-004 | Build desde cero | PENDIENTE |
| B2-005 | Build incremental | PARCIAL |
| B2-006 | Runner `cmake --build` | PARCIAL |
| B2-007 | Runner Ninja directo | PENDIENTE |
| B2-008 | Runtime host STATIC/DYNAMIC | PENDIENTE |
| B2-009 | Optimización host O0/O1/O2/O3 | PENDIENTE |
| B2-010 | Identidad, ruta y SHA-256 del compilador | PARCIAL; sólo por corrida |
| B2-011 | Recibo persistente del build | PENDIENTE |
| B2-012 | Instalar build válido | PENDIENTE; implementar sin ejecutar en C: |
| B2-013 | Selección explícita de prefijo | PARCIAL |
| B2-014 | Pruebas bloqueadas sin compilador construido válido | PENDIENTE |
| B2-015 | Selección O0 | VERIFICADA |
| B2-016 | Selección O1 | PENDIENTE; implementar sin ejecutar |
| B2-017 | Selección O2 | PENDIENTE; implementar sin ejecutar |
| B2-018 | Selección O3 | PENDIENTE; implementar sin ejecutar |
| B2-019 | Selección de todos los perfiles | PENDIENTE; implementar sin ejecutar |
| B2-020 | Caso NativeSharp individual | VERIFICADA O0 |
| B2-021 | Categoría NativeSharp | VERIFICADA O0 |
| B2-022 | Batería NativeSharp | VERIFICADA O0 |
| B2-023 | Registro unitario consolidado | VERIFICADA |
| B2-024 | Suites especializadas | VERIFICADA O0 |
| B2-025 | NSLink explícito y configuración Release/Debug | PARCIAL |
| B2-026 | Construcción única antes de ejecutar la matriz | VERIFICADA O0 |
| B2-027 | Progreso por etapa | VERIFICADA básica |
| B2-028 | Salida completa | VERIFICADA |
| B2-029 | Errores causales condensados | VERIFICADA básica |
| B2-030 | Resultados por perfil y capa | PENDIENTE |
| B2-031 | Historial persistente | VERIFICADA |
| B2-032 | Repetir únicamente fallos | PENDIENTE |
| B2-033 | Limpieza selectiva de historial | PENDIENTE |
| B2-034 | Limpieza segura de builds administrados | PENDIENTE |
| B2-035 | Cancelación sólo de descendientes | VERIFICADA |
| B2-036 | Reutilización por identidad exacta | PENDIENTE |
| B2-037 | Conservación opcional de temporales | PENDIENTE |
| B2-038 | Archivo histórico ABI | PENDIENTE; implementar sin ejecutar |
| B2-039 | Baseline/certificación | PENDIENTE; implementar sin ejecutar |
| B2-040 | CLI automatizable | PARCIAL |
| B2-041 | Configuración versionada y variables de entorno | PENDIENTE |
| B2-042 | Builder anterior completamente intacto | VERIFICADA |
| B2-043 | Área Toolchain separada en la GUI | PENDIENTE |
| B2-044 | Construcción NSLink Release/Debug | IMPLEMENTADA; ejecución pendiente |
| B2-045 | Construcción NSASM Release/Debug | IMPLEMENTADA; ejecución pendiente |
| B2-046 | Construcción NSDBG Release/Debug | IMPLEMENTADA; ejecución pendiente |
| B2-047 | Rutas fuente, build y salida configurables por herramienta | IMPLEMENTADA en modelo |
| B2-048 | Recibo persistente y SHA-256 por herramienta | IMPLEMENTADA y verificada con fixtures |
| B2-049 | Staging de `nslink.exe` junto al NSC construido en A: | IMPLEMENTADA y verificada con fixture |
| B2-050 | Staging de `nsasm.exe`, `nsdump.exe` y `nsdis.exe` en A: | IMPLEMENTADA y verificada con fixture |
| B2-051 | Staging de `nsdbg.exe` y PDB disponible en A: | IMPLEMENTADA; fixture GUI pendiente |
| B2-052 | Selección de herramientas incluidas en deploy | PENDIENTE |
| B2-053 | Autogeneración seleccionable antes del deploy | PENDIENTE |
| B2-054 | Deploy exclusivamente mediante acción explícita | PENDIENTE; implementar sin ejecutar en C: |
| B2-055 | Manifiesto de staging/deploy con procedencia y hash | PARCIAL; staging implementado |
| B2-056 | Fallo productor bloquea staging y deploy dependientes | IMPLEMENTADA en recibos |
| B2-057 | CMake/Ninja/compiladores resueltos por PATH o proyecto | IMPLEMENTADA en planes de herramientas |
| B2-058 | NSC y herramientas conservan configuración Release/Debug elegida | IMPLEMENTADA en modelo |
| B2-059 | Instalación conjunta sin leer artefactos desde C: | PENDIENTE; implementar sin ejecutar en C: |

## Gate de cierre corregido

La tanda 20 no volverá a cerrarse hasta cumplir:

```ini
capacidades_inventariadas = 59
capacidades_implementadas + retiradas_explicitas = 59
capacidades_omitidas = 0
reducciones_inferidas = 0
builder_anterior_modificado = 0
```

Las capacidades no autorizadas para ejecución deben quedar implementadas y
etiquetadas como `IMPLEMENTADA; VALIDACIÓN NO EJECUTADA POR RESTRICCIÓN`.

## Restricciones conservadas

- No modificar `nscbuilder.py` ni `_nscbuilder`.
- No modificar el compilador.
- No escribir ni desplegar en `C:`.
- No ejecutar baseline, certificación, promoción, O1, O2 ni O3.
- No hacer push ni operaciones remotas.
