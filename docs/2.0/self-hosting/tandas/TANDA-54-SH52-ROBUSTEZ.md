# Tanda 54 — SH52: robustez terminal

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | `NS2-SH-408`–`NS2-SH-414` |
| Gate | `SH52` |
| Rama | `t54-staging` → `development-claude` |
| Commit de entrada | T53/SH51 `6b273e5f2` |
| Commit de cierre | este commit (`selfhost(tanda-54): certify hostile-input robustness`) |
| Fecha de cierre | 2026-09-22 |
| Baseline de cierre | acumulado en T56 por excepción terminal aprobada |

## 2. Objetivo, alcance y diseño

Probar que entradas hostiles y fallos externos terminan de forma acotada, diagnosticable y sin
publicar artefactos parciales. El gate usa un corpus pseudoaleatorio con semilla fija; rechaza una
NSMETA truncada; fuerza el timeout durante la compilación real de las 80 unidades de runtime;
interrumpe desde el sistema un lexer activo; presenta un directorio donde se esperaba un `.obj`;
compara dos objetos O0 y audita fuentes/binario contra `MessageBoxA/W`.

No cambia el lenguaje, ABI ni código del compilador: las defensas existentes pasaron el contrato
terminal. La prueba dirigida es acumulativa respecto de SH51 y el baseline costoso queda reservado
a T56.

## 3. Contratos e invariantes

- Todo fuzz termina dentro de 20 segundos y sólo devuelve éxito o error de compilación.
- Metadata corrupta nunca se acepta.
- Timeout e interrupción no dejan el producto solicitado.
- Un fallo de publicación no reemplaza un directorio ni deja contenido parcial.
- Dos compilaciones limpias O0 del mismo proyecto son idénticas por SHA-256.
- El compilador no enlaza ni menciona `MessageBoxA/W`; errores permanecen en consola/log.
- El directorio de evidencia no contiene `.tmp` o `.temp` al terminar.

## 4. Archivos, tipos y funciones

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `selfhost/contracts/robustness-final.contract.json` | crear | corpus, presupuestos y matriz SH52 |
| `selfhost/bootstrap/verify_robustness_final.py` | crear | ejecutor hostil y reporte determinista |
| `cmake/VerifyNativeSharpRobustnessFinal.cmake` | crear | aislamiento y entrada CMake del gate |
| `CMakeLists.txt` | modificar | test acumulativo después de SH51 |
| roadmap, expediente y resumen SH52 | crear/modificar | cierre normativo |

No se añaden tipos públicos. `run(command, cwd, timeout)` captura procesos sin elevar excepciones por
código de salida; `digest(path)` fija identidad SHA-256; `main()` valida contrato, ejecuta las ocho
dimensiones, invoca la expresión CTest exacta y siempre escribe el reporte antes de devolver fallo.
Los procesos interrumpidos reciben terminate, espera acotada y kill de respaldo; todo archivo vive
dentro del `OUTPUT_DIR` suministrado por CMake.

## 5. Pruebas y evidencia

| Dimensión | Resultado |
|---|---|
| fuzz determinista | 32/32 host, 8/8 autoalojado |
| metadata hostil | rechazada |
| timeout cooperativo | cancelado antes de `syntax`, sin `.obj` |
| interrupción externa | proceso terminado y cosechado |
| destino hostil | error atómico, directorio intacto y vacío |
| reproducibilidad O0 | objetos idénticos, SHA-256 `c660c585b50bfb5e4703b069a294178436b8b43e80065f1112326be6a6d6b0a7` |
| MessageBox/temporales | cero/cero |
| CTests dirigidos | 7/7 |

Los CTests cubren `compiler-hardening`, `frontend-robustness`, dependencias/cancelación de jobs,
comptime reproducible y tres contratos de metadata. Reporte dirigido SHA-256
`4a08d1e9cd9465d6b8c3384523e10b1390d6c06802fb1e1175980889ccb3965e`; contrato
`c6066d42d50d11378d8dce783c73366be4fc37dac66f44f8082879e319dd77cd`; verificador
`1dd9923f5a620a22a56018d161d1e7636688580162601011bed1f0900f4a4842`.

## 6. Baseline, riesgos e higiene

El baseline completo no se ejecuta en T54: por la excepción terminal documentada, el gate dirigido
se incorpora al único baseline total de T56. El reporte no contiene fallos ni omitidos. El riesgo
residual es específico de plataforma: la auditoría `MessageBox` observa fuentes y símbolos literales
del PE; la matriz final vuelve a ejecutar el comportamiento real con ambos linkers.

- [x] gate SH52 dirigido en verde;
- [x] reporte y hashes conservados;
- [x] cero ejecutables, objetos, logs o temporales en el commit;
- [x] baseline acumulativo reservado a T56;
- [x] commit único de tanda.
