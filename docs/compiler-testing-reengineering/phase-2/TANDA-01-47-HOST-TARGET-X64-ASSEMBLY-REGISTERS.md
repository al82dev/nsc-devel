# Tanda 01.47 — Reconocimiento de registros assembly x64 de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-028`, casos `TF-TGT-217..224` |
| Producto | Observar `TargetInfo::isAssemblyRegister` para Windows x64; no modificar |
| Excluido | Registros ARM64, requisitos de mnemónicos y validación del parser de assembly |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- `rax` demuestra el reconocimiento mínimo del catálogo x64;
- aliases de byte, registros r8-r15 con variantes de ancho, XMM, MMX y stack x87 cubren familias representativas;
- el factory x64 predeterminado publica el catálogo completo sin metadata externa;
- mayúsculas, prefijos o sufijos inválidos, familias no soportadas y ordinales desbordados se rechazan;
- los límites comprueban r7/r8/r15/r16, xmm0/xmm15/xmm16, mm0/mm7/mm8 y st0/st7/st8;
- el corpus completo contiene exactamente 100 nombres válidos y 23 inválidos explícitos;
- recorrido directo, recorrido inverso y repetición conservan cada veredicto;
- 65,536 consultas iterativas recorren cinco familias válidas y el corpus inválido.

El oráculo enumera de forma independiente los 36 aliases históricos, los sufijos de r8-r15, los rangos de XMM,
MMX y stack x87, y los 23 nombres inválidos. No deriva expectativas del producto. La matriz completa cuesta tiempo
lineal respecto al corpus; la prueba de escala usa tiempo lineal y memoria auxiliar acotada. No existe recursión.

## Implementación de pruebas

- `TargetX64AssemblyRegisterModule.cpp` registra los ocho casos dentro del ejecutable consolidado.
- `run_host_target_x64_assembly_register_gate.py` exige exactamente `TF-TGT-217..224` y ocho aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CF-TGT-028 -> Host.Target` para Debug y Release.
- CMake registra el módulo y su gate dirigido sin crear un ejecutable por prueba.

El módulo sólo consulta la API pública. No altera estado de producto, no usa configuración externa y no comparte
estado mutable entre casos. El controlador crea un directorio temporal aislado y verifica identidad y conteos.

## Evidencia

- Gate `CF-TGT-028`: 8/8 en Debug (2.02 s) y Release (0.45 s).
- Suite nueva completa: Debug 46/46 en 20.06 s; Release 46/46 en 9.38 s.
- Controlador Python: 51/51 en 1.958 s; fuentes Python: 67/67 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 354 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-217` | registro mínimo `rax` |
| `TF-TGT-218` | familias y aliases representativos |
| `TF-TGT-219` | catálogo disponible sin metadata externa |
| `TF-TGT-220` | estructuras, capitalización y ordinales inválidos |
| `TF-TGT-221` | límites exactos de las cuatro familias numeradas |
| `TF-TGT-222` | corpus completo de 100 válidos y 23 inválidos |
| `TF-TGT-223` | determinismo, repetición y orden inverso |
| `TF-TGT-224` | escala iterativa de 65,536 consultas |

Diagramas: no aplica; el contrato observado es una consulta booleana sin estado y el corpus enumerado expresa
completamente sus familias y límites.
