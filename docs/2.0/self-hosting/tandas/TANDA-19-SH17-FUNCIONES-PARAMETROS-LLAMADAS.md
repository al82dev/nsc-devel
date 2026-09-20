# NativeSharp 2.0 self-hosting — tanda 19: funciones, parámetros y llamadas

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | ampliación vertical del backend derivado de fuente |
| Gate | `SH17` |
| Rama | `development` |
| Milestone | `nativesharp-2.0-selfhost-milestone` |
| Commit de entrada | `b0f6dd902` |
| Commit de cierre | este commit (`selfhost(tanda-19): lower source functions parameters and calls`) |
| Fecha de apertura/cierre | 2026-09-20 |
| Baseline de cierre | `tanda-19-sh17-source-functions-20260920` |

## 2. Objetivo y alcance

SH17 elimina la restricción de una sola expresión aislada en `Main`. El backend declara funciones
`int`, emite sus cuerpos de retorno, obtiene parámetros desde la función LLVM y baja llamadas
directas con argumentos. El caso observable es `Add(19, 23)`, ejecutado como 42 en O0, O1, O2 y O3
con `lld-link` y NSLink.

Se incluyen funciones libres o métodos que ya tengan forma AST compatible, retorno directo `int`,
entre cero y cuatro parámetros `int`, expresiones aritméticas de SH16 y llamadas directas por
identificador. No se incluyen variables locales, asignación, ramas, bucles, memoria, sobrecargas,
recursión certificada, métodos con receptor, agregados, excepciones ni compilación del proyecto del
compilador por el backend. `--bootstrap-generation` continúa cerrado con código 78.

## 3. Diseño

### Arquitectura y flujo

`DeclareFunctions` recorre primero el árbol completo y registra cada firma LLVM; esta primera fase
permite llamadas hacia funciones declaradas después. `EmitFunctions` realiza un segundo recorrido,
crea un bloque por cuerpo y entrega el retorno a `EmitI32Expression`. Para un identificador, el
backend consulta `SemanticFact.SymbolId()` —la identidad de usos de expresión— y la compara con
`SemanticModel.NodeSymbol()` de cada declaración de parámetro. Para una llamada, resuelve la
declaración fuente, comprueba aridad, localiza la función LLVM y baja los argumentos antes de
construir `LLVMBuildCall2`. `NativeSharpCompute` llama finalmente al `Main` fuente.

### Contratos e invariantes

- Cada función soportada tiene una firma LLVM `i32(i32...)` y un nombre
  `__nativesharp_source_<nombre>`.
- Se declaran todas las firmas antes de emitir cualquier cuerpo.
- Una aridad mayor de cuatro, un retorno ausente o una expresión fuera del subconjunto falla sin
  crear un objeto válido; no existe fallback constante ni objeto canónico.
- La identidad de un uso de parámetro procede del análisis semántico, no de comparar sólo texto.
- O0–O3 deben preservar el resultado y ambos linkers deben consumir los mismos objetos.
- Toda llamada a LLVM cruza `NativeSharp.LLVM.C`; no se incluye layout C++.

| Decisión | Alternativas | Razón |
|---|---|---|
| Dos recorridos declaración/emisión | emitir al encontrar cada función | admite llamadas hacia delante y separa firmas de cuerpos |
| Símbolo del hecho semántico para usos | `NodeSymbol` o nombre textual | `NodeSymbol` enlaza declaraciones; el hecho semántico enlaza usos y respeta scopes |
| Límite temporal de cuatro parámetros | memoria dinámica inmediata | mantiene el perfil bootstrap pequeño y el contrato explícito |
| Código interno 13/14 | reutilizar 3 | distingue fallo de declaración de fallo de cuerpo durante el bootstrap |

## 4. Inventario de archivos

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | modificar | declarar y emitir funciones, parámetros y llamadas |
| `lib/NativeSharp.LLVM.C/LLVMApi.ns` | modificar | exponer `LLVMGetNamedFunction` |
| `selfhost/contracts/llvm-c-api.contract.json` | modificar | congelar hash y 44 símbolos bootstrap |
| `selfhost/tests/codegen/source-functions.ns` | crear | fixture `Add(19, 23)` |
| `selfhost/contracts/source-functions.contract.json` | crear | contrato ejecutable SH17 |
| `selfhost/bootstrap/verify_source_functions.py` | crear | anatomía, hashes y ocho ejecuciones |
| `cmake/VerifyNativeSharpSourceFunctions.cmake` | crear | construir candidatos, objetos y PEs |
| `CMakeLists.txt` | modificar | registrar `nsc.selfhost.source-functions` |
| `tests/selfhost/test_bootstrap_infrastructure.py` | modificar | regresión de contrato y resolución semántica |
| documentación SH17 | crear/modificar | diseño, límites y operación acumulativa |

No se introduce un tipo público nuevo. `LLVMGetNamedFunction(void*, byte*): void*` amplía la
frontera externa. El contrato `nativesharp-selfhost-source-functions` versión 1 añade perfiles,
linkers, máximo de parámetros, símbolos y APIs requeridas.

## 5. Métodos y funciones

| Firma | Responsabilidad | Entradas/salida | Errores, efectos y ownership |
|---|---|---|---|
| `ParameterCount(SyntaxTree,int): int` | contar parámetros declarados | declaración válida → aridad | sólo lectura, sin ownership |
| `I32FunctionType(void*,int): void*` | construir firma LLVM | i32 y aridad 0–4 → tipo o null | puntero propiedad de LLVMContext |
| `LlvmFunctionName(string): owned string` | nombre estable de símbolo | nombre fuente → copia prefijada | devuelve ownership al llamador |
| `FindFunctionDeclaration(SyntaxTree,int,string): int` | resolver declaración por nombre en la rebanada | raíz/nombre → id o -1 | sólo lectura; sobrecargas fuera de alcance |
| `ReturnExpression(SyntaxTree,int): int` | localizar retorno directo | declaración → expresión o -1 | sólo lectura |
| `DeclareFunctions(void*,void*,SyntaxTree,int): bool` | predeclarar firmas | módulo/tipo/árbol | muta módulo; false en firma no soportada |
| `EmitI32Expression(...)` | bajar literales, parámetros, aritmética y llamadas | AST/semántica/función → LLVMValueRef | muta builder; `valid=false` al salir del subconjunto |
| `EmitFunctions(...)` | emitir bloques y retornos | módulo y árbol declarado | muta módulo; false ante cuerpo inválido |
| `verify_source_functions.main(): int` | verificar matriz SH17 | contrato, backend y artefactos | escribe reporte; 1 ante primera divergencia |

Los arreglos temporales de tipos y argumentos viven sólo durante la llamada LLVM-C. El módulo es
single-thread y se destruye por `Dispose`; no quedan punteros al AST ni buffers temporales después
de `EmitExecutableObject`.

## 6. Superficie observable e implementación

La CLI no cambia: `--emit-object <source> -O0|-O1|-O2|-O3 -o <path>`. Los códigos internos 13 y
14 separan error al declarar firmas y error al emitir cuerpos; el driver mantiene el diagnóstico
general de backend y salida distinta de cero. El reporte nuevo usa schema
`nativesharp-selfhost-sh17-report`, versión 1, gate `SH17` y scope exacto
`i32-functions-parameters-calls`.

La primera ejecución dirigida devolvió código interno 3. Las llamadas sin parámetros y los
parámetros no usados sí emitían objeto; un retorno que usaba el parámetro fallaba. La causa fue
consultar `NodeSymbol(syntaxId)` en un nodo de uso: esa tabla enlaza declaraciones, mientras el uso
guarda su símbolo en `Fact(syntaxId).SymbolId()`. La corrección conserva resolución semántica y
añade una aserción estructural específica. No se alteró la fixture para esconder el fallo.

## 7. Pruebas y comparación diferencial

| Prueba | Tipo | Propósito | Resultado/Evidencia |
|---|---|---|---|
| `test_tanda19_source_functions_use_llvm_parameters_and_calls` | regresión | contrato, APIs y origen correcto del símbolo | 37/37 unitarias |
| `nsc.selfhost.source-functions` | positiva/integración | O0–O3 × dos linkers, símbolos y salida 42 | reportes SH17 |
| `source-derived-backend` | regresión | conservar fuentes diferenciales de SH16 | aprobada en ambos árboles |
| `project-driver`, `first-generation`, `llvm-c-bridge` | acumulativa | no romper fronteras anteriores | aprobadas en ambos árboles |

No existe una superficie equivalente completa en `nsc0` para este backend parcial. La comparación
observable exige ABI PE/COFF consumible por ambos linkers y resultado idéntico 42; la anatomía
exige `__nativesharp_source_Add`, `__nativesharp_source_Main` y `NativeSharpCompute`. No se autoriza
normalización de objetos ni sustitución por artefactos de `nsc0`.

## 8. Baseline de cierre

```bat
python nscbuilder.py --baseline --jobs 16 --prefix A:\native-sharp ^
  --build-dir A:\nsc-devel\selfhost\out\nsc0-build ^
  --test-build-lld A:\nsc-devel\selfhost\out\tests-lld ^
  --test-build-nslink A:\nsc-devel\selfhost\out\tests-nslink ^
  --llvm-root C:\clang-llvm --nslink A:\native-sharp\bin\nslink.exe ^
  --history-root A:\nsc-devel\selfhost\evidence\runs ^
  --work-root A:\nsc-devel\selfhost\out\work ^
  --temp-root A:\nsc-devel\selfhost\out\temp --write-root A:\ ^
  --run-id tanda-19-sh17-source-functions-20260920 ^
  --build-id 2000000000000000
```

La matriz esperada y obligatoria es 452/452 CTest por enlazador, 148/148 fixtures y 18/18 triage
por enlazador. G00 y el reporte JSON de la corrida son autoritativos. La falla dirigida anterior no
fue un baseline completo y queda descrita arriba; si el baseline completo falla, su corrida se
conservará y se ejecutará uno nuevo después de corregir la causa.

Resultado: `PASSED` en 844.278 s, 452/452 CTest por enlazador, 148/148 fixtures y 18/18
triage por enlazador. El digest de fuentes observado por G00 fue
`1f2f3b853cd95b872fbdeffe4244f0f1b9366f3c92702721964306a098a7daf7`; los hashes de reportes y
del gate quedan congelados en `selfhost/evidence/gates/SH17/gate.json`.

## 9. Riesgos, deuda e higiene

La búsqueda por nombre aún no implementa overload sets; no se certifican recursión, tipos distintos
de i32 ni parámetros por referencia. El límite cuatro es deuda explícita. Los recorridos visitan el
árbol completo y priorizan claridad sobre rendimiento. SH17 no demuestra que las 21 fuentes del
compilador estén dentro del backend y no permite promover `nsc-next`.

El commit excluye `selfhost/out`, objetos, ejecutables, DLL, LIB, PDB y temporales. También excluye
los cambios ajenos ya presentes en `docs/1.0.1`, `mkdown/` y `Claude outputs/`. El manifiesto de
fuentes no cambia porque la nueva fixture no pertenece al conjunto congelado del compilador.

## 10. Criterio de cierre

- [x] Funciones, parámetros y llamadas se derivan de fuente real.
- [x] La prueba dirigida pasa O0–O3 con LLD y NSLink.
- [x] El baseline completo de cierre está verde.
- [x] SH17 tiene reportes y gate versionados sin problemas.
- [x] El expediente coincide con métodos, archivos y límites implementados.
