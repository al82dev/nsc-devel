# Tanda 01.48 — Reconocimiento de registros assembly ARM64 de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-029`, casos `TF-TGT-225..232` |
| Producto | Observar `TargetInfo::isAssemblyRegister` para Windows ARM64; no modificar |
| Excluido | Registros x64, requisitos de mnemónicos y validación del parser de assembly |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- `sp` demuestra el reconocimiento mínimo del catálogo ARM64;
- `sp`, `fp`, `lr`, `xzr`, `wzr` y los extremos de x, w y v cubren las familias representativas;
- el factory ARM64 publica el catálogo completo sin metadata externa;
- ausencia de dígitos, texto sobrante, mayúsculas, familias no soportadas y ordinales desbordados se rechazan;
- los límites comprueban x0/x30/x31, w0/w30/w31 y v0/v31/v32;
- el corpus completo contiene exactamente 99 nombres válidos y 18 inválidos explícitos;
- recorrido directo, recorrido inverso y repetición conservan cada veredicto;
- 65,536 consultas iterativas recorren aliases, tres familias numeradas y el corpus inválido.

El oráculo enumera independientemente los cinco aliases, los rangos x0-x30, w0-w30 y v0-v31, y los 18 nombres
inválidos. No deriva expectativas del producto. La matriz completa cuesta tiempo lineal respecto al corpus; la
prueba de escala usa tiempo lineal y memoria auxiliar acotada. No existe recursión.

## Implementación de pruebas

- `TargetArm64AssemblyRegisterModule.cpp` registra los ocho casos dentro del ejecutable consolidado.
- `run_host_target_arm64_assembly_register_gate.py` exige exactamente `TF-TGT-225..232` y ocho aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CF-TGT-029 -> Host.Target` para Debug y Release.
- CMake registra el módulo y su gate dirigido sin crear un ejecutable por prueba.

El módulo sólo consulta la API pública. No altera estado de producto, no usa configuración externa y no comparte
estado mutable entre casos. El controlador crea un directorio temporal aislado y verifica identidad y conteos.

## Evidencia

- Gate `CF-TGT-029`: 8/8 en Debug (1.99 s) y Release (0.49 s).
- Suite nueva completa: Debug 47/47 en 20.55 s; Release 47/47 en 8.96 s.
- Controlador Python: 51/51 en 0.682 s; fuentes Python: 68/68 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 362 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-225` | registro mínimo `sp` |
| `TF-TGT-226` | aliases y familias representativas |
| `TF-TGT-227` | catálogo disponible sin metadata externa |
| `TF-TGT-228` | estructura, texto sobrante, capitalización y ordinales inválidos |
| `TF-TGT-229` | límites exactos de x, w y v |
| `TF-TGT-230` | corpus completo de 99 válidos y 18 inválidos |
| `TF-TGT-231` | determinismo, repetición y orden inverso |
| `TF-TGT-232` | escala iterativa de 65,536 consultas |

Diagramas: no aplica; el contrato observado es una consulta booleana sin estado y el corpus enumerado expresa
completamente sus familias y límites.
