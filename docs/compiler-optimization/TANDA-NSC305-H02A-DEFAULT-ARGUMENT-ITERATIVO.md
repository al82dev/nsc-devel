# NSC 3.0.5 — H02A: optional-default lowering iterativo

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la autorrecursión de `lowerDefaultArgument`. |
| Algoritmo | Recolectar frames unary en un vector y reconstruir el HIR desde la hoja hacia la raíz. |
| Protegido | Tipos de constantes, token de cada unary, conversiones de string, diagnostics y orden del árbol. |
| Fuera de alcance | Máquina general de statements/expressions, parser de unary, MIR, LLVM y nuevos defaults válidos. |
| Terminación | Cero autocalls, profundidad 32,768, contrato de fuente permitido, focos HIR y `nsc`. |

## Resultado

- `lowerDefaultArgument` ya no se llama a sí mismo.
- La hoja literal conserva exactamente la misma selección de nodo y conversión.
- Los wrappers unary se reconstruyen en orden inverso, equivalente al unwind de la recursión
  anterior: el primer nodo observado por el llamador sigue siendo el unary exterior.
- No se amplió el lenguaje. Sema continúa rechazando defaults que no sean una constante literal
  válida; el caso público `-1` sigue siendo aceptado y produce exactamente un nodo unary.
- El acceso amigo `ProjectOopHirModelTestAccess` existe sólo para probar directamente el helper
  privado sin depender del límite semántico anterior a HIR.

## Contrafactual profundo

La prueba directa construye un AST sintético de 32,768 unary nodes sobre una constante entera,
ejecuta el helper privado y comprueba iterativamente:

- éxito sin stack overflow;
- 32,768 nodos `Unary` conservados;
- hoja `IntegerConstant(1)` conservada;
- desmontaje iterativo del resultado para que el destructor de la prueba no oculte otra recursión.

La primera tentativa de ejercer esa profundidad mediante fuente fue rechazada correctamente antes
de HIR con `default parameter value must be a literal constant`. No se relajó esa regla para hacer
pasar la prueba; se separó el contrato público de la prueba estructural interna.

## Evidencia focal

- Foco `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-default-argument-depth`: `PASS 4/4`, exit `0`.
- Profundidad directa: 32,768 frames.
- Fuente pública permitida `int input = -1`: llega a OOP HIR y conserva un unary.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.
- `git diff --check`: limpio.

H02 permanece abierto para los escaneos async y la máquina iterativa general. No se ejecutó
baseline, deploy ni batería completa.
