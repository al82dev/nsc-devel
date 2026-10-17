# NSC 3.0.5 — G10: cadena calificada profunda en `generics`

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`,
  `744f93710c4499876a2e4907199f831b56c71fde`.
- **Entrada roja exacta:** el proyecto focal conservado en
  `A:\nsc-devel\_namespace_function_lookup_test\DeepExpressionName\DeepExpressionName.nsproj`,
  cuya expresión calificada contiene 16,384 segmentos.
- **Defecto demostrado:** `nsc --check-project-members ... -O0 -j 1 --progress` termina
  `syntax-parse`, entra en `generics` y finaliza con `0xC00000FD` antes de construir el modelo de
  funciones de namespace.
- **Naturaleza:** corrección interna de overflow mediante recorrido iterativo; no es una extensión
  del lenguaje ni una optimización de LLVM.
- **Causa atribuida:** `ProjectGenericModel::inferGenericCall` llama
  `receiverSourceSpelling`; este helper descendía recursivamente por cada `MemberExpression` y
  reconstruía un prefijo nuevo al regresar de cada nivel. La entrada combina 16,384 marcos nativos
  con concatenación cuadrática.
- **Instrumentación temporal autorizada:** reactivar los breadcrumbs detallados ya conservados en
  `Driver.cpp`, `ProjectCompilation.cpp`, `ProjectGenericModel.h` y `ProjectGenericModel.cpp` para
  escribir `A:\temp\nsc-generics-trace.log`. Debe retirarse por completo antes del commit
  correctivo.
- **Archivos de producción autorizables tras la atribución:** exclusivamente el archivo y símbolo
  que contengan el recorrido causal demostrado. Cualquier archivo adicional requiere ampliar este
  presupuesto con evidencia.
- **Prueba focal autorizada:** el target unitario que ya contiene la regresión de nombres
  calificados y la invocación directa del proyecto rojo con el `nsc.exe` construido en A:.
- **Contratos congelados:** sintaxis, semántica, diagnósticos, orden de recorrido, resolución de
  nombres, API, ABI, NSMETA, CLI, artefactos, paralelismo y formato de salida normal.
- **Protegidos:** parser, B09B, miembros, HIR, MIR, LLVM, linker, runtime, Builder/GUI y
  consumidores externos.
- **Prohibido:** atribuir la caída a B09B, generalizar otros walkers sin reproducción propia,
  baseline, suite completa, deploy, instalación, red o cualquier escritura fuera de A:.
- **Gate:** checkpoint causal exacto; reproducción roja focal; corrección iterativa mínima; prueba
  verde profunda de la fase `generics`; build focal de `nsc`; diff auditado; tracking actualizado
  y commit local aislado. El overflow posterior de `names` se transfiere a N01 y no se mezcla con
  esta hipótesis.

## Cadena causal

```text
CallExpression con receptor de 16,384 MemberExpression
→ ProjectGenericModel::processNodeOnce
→ ProjectGenericModel::inferGenericCall
→ receiverSourceSpelling(callee->first())
→ una llamada nativa por segmento + reconstrucción repetida de prefijos
→ 0xC00000FD dentro de generics.root-demand-discovery
```

La traza crash-surviving terminó exactamente en
`generics.receiver-source-spelling`; todas las subfases anteriores y el checkpoint de
`infer-generic-call` habían abierto correctamente. La evidencia se conservó durante el diagnóstico
en `A:\temp\nsc-generics-trace.log` y la instrumentación temporal fue retirada antes del build
verde.

## Implementación

`receiverSourceSpelling` recorre iterativamente la espina izquierda del receptor, conserva las
vistas de los miembros, calcula el tamaño final, reserva una sola vez y ensambla desde el
identificador raíz hacia el miembro exterior. La función conserva el retorno vacío para un receptor
nulo, una raíz que no sea identificador o una raíz de texto vacío. No cambia resolución, orden ni
formato del nombre escrito.

## Evidencia focal

- Rojo original: `syntax-parse` termina; `generics` cae con `0xC00000FD`.
- Primera traza: la última subfase abierta es `generics.root-demand-discovery`.
- Segunda traza: el último checkpoint abierto es `generics.receiver-source-spelling`.
- Contrafactual con el helper iterativo: `generics` termina en 145 ms sobre la misma entrada.
- Regresión focal limpia: `nsc_namespace_function_lookup_tests.exe` — `PASS: 12 checks`.
- Build focal limpio de `nsc.exe`: terminado; contrato de host runtime estático aprobado.
- Búsqueda estática: no queda auto-llamada de `receiverSourceSpelling`.
- Instrumentación temporal: retirada por completo; los cuatro archivos de soporte preservado no
  tienen diferencial.

## Transferencia no ocultada

Al continuar la misma invocación hasta `--check-project-members`, `generics` termina y la siguiente
fase pública, `names`, cae antes de terminar. Es un productor distinto y queda activo como N01; no
invalida la corrección ni autoriza mezclar `ProjectNameLookup` en el commit G10.

## Estado

G10 terminado. N01 activo.
