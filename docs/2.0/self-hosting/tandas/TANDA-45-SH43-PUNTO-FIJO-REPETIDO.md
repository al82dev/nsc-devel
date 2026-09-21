# Tanda 45 — SH43: punto fijo repetido

| Campo | Valor |
|---|---|
| Estado | `LANDED` — gate dirigido y baseline acumulado aprobados |
| Dependencia | SH42 `f83fe40da` |
| Gate | SH43 |
| Instalación de trabajo | `A:\native-sharp` |
| LLVM | `C:\clang-llvm` |

## Objetivo

SH41 observó un punto fijo una vez. SH43 eleva esa observación a una propiedad reproducible: tres
raíces nuevas por perfil deben producir exactamente los mismos objetos e imágenes al recorrer
`nsc2 → nsc3 → nsc4`. Ejecutar bien no basta y tampoco se autoriza normalizar el resultado.

## Diseño e implementación

`verify_repeated_fixed_point.py` crea doce raíces independientes —tres por O0, O1, O2 y O3— y
procesa cuatro casos simultáneos. `execute_case()` realiza en cada raíz:

1. `nsc2 --emit-bootstrap-object` produce el objeto de `nsc3`;
2. LLD y NSLink producen las dos imágenes `nsc3`;
3. la imagen LLD de `nsc3` produce el objeto de `nsc4`;
4. ambos objetos se comparan con `read_bytes()`;
5. ambos linkers producen `nsc4` y cada imagen se compara con su homóloga `nsc3`;
6. identidad, self-test y proyecto completo se verifican en ambas generaciones;
7. al finalizar, los hashes de las tres raíces se comparan por perfil y enlazador.

La primera corrida reveló una diferencia en la imagen LLD aunque los objetos eran idénticos. La
causa era el timestamp PE generado al enlazar. `link()` ahora pasa `/timestamp:0`; esto elimina la
variación durante la producción. No se abre, modifica ni reescribe el ejecutable después del enlace.
NSLink ya produjo bytes deterministas sin opción adicional.

## Archivos y funciones

| Archivo | Responsabilidad |
|---|---|
| `selfhost/bootstrap/verify_repeated_fixed_point.py` | raíces limpias, concurrencia, generaciones, enlace y comparación cruda |
| `cmake/VerifyNativeSharpRepeatedFixedPoint.cmake` | limpieza dirigida, candidatos SH41 y bibliotecas de runtime/importación |
| `selfhost/contracts/repeated-fixed-point.contract.json` | repeticiones, perfiles, linkers y política de identidad |
| `CMakeLists.txt` | dependencia SH42 → SH43 → promotion-readiness y ejecución serial del gate |

Funciones principales: `run`, `checked`, `link`, `verify_image`, `execute_case` y `main`.

## Gate dirigido

| Medición | Resultado |
|---|---:|
| raíces limpias / casos | 12 / 12 |
| perfiles / linkers | 4 / 2 |
| comparaciones de objeto | 24 |
| comparaciones de imagen | 48 |
| normalizaciones | 0 |
| omitidos / problemas | 0 / 0 |
| tiempo | 361.21 s |

| Perfil | SHA-256 objeto | SHA-256 LLD | SHA-256 NSLink |
|---|---|---|---|
| O0 | `8c4d695e00ff8edfcaad07752a238bbd729987ed317ff57a6f0d65467fe4b27f` | `788b7f60613dfa3309e6c1fe08d1798e103c3c6c0984d4b11ee44a800281dadf` | `4d78749b5c97aaadd142a6bbf695eaa27ab06cf149964effebde1d9b3a9fba98` |
| O1 | `1675e571c937759b9d220360564c8803359cb75c408f7a83885a5162aec2075f` | `612ec271dd098975927a933db3c2b9cd18f4c52fd1e2bbd305476b415239f8fa` | `26abf6be313fb2baf604f3fad172c292856b9ceea97a3e449c7a71e7803b972c` |
| O2 | `e0b5da15155d6d95c67c67d06af2e0b87399a68b7bb05c4c45fc36f8995c3647` | `c82b5311199759952c79ffb767f19f0abef50cd9fae57a657e5266fbeb5b4590` | `6518bb31f7587660b5f00c09324fa010796dc6a930d2967d1b02ffc2459905f1` |
| O3 | `8c1d6f9179ace3b8c649a3de0c2d34150d92bc16268b51f42e32a47d24933f04` | `cfe7c2ade0750c85f0298389bd29863a3eac9709f3811b6509170893fe62b1b0` | `2579157553bcd1898aea4326ce400f63f0574d7c1354cecaf1a7ffc5e1d65b45` |

## Baseline acumulado

Corrida: `selfhost/evidence/runs/tanda-45-sh43-punto-fijo-20260921/report.md`.

| Componente | Resultado | Tiempo |
|---|---:|---:|
| configuración, construcción e instalación | aprobada | 2.278 s |
| construcción CTest LLD / NSLink | aprobada | 6.857 / 7.484 s |
| CTest LLD | 478/478 | 1623.034 s |
| fixtures / triage LLD | 148/148 · 18/18 | 50.337 / 8.346 s |
| CTest NSLink | 478/478 | 1567.228 s |
| fixtures / triage NSLink | 148/148 · 18/18 | 46.907 / 7.713 s |
| Gate 0 | `LANDED` | 9.557 s |
| total | 0 fallos | 3344.791 s |

La instalación de trabajo quedó en `A:\native-sharp` con build reproducible
`4500000000000001`. Los objetos, ejecutables, logs y temporales permanecen ignorados.

## Criterios de cierre

- [x] tres raíces limpias por perfil;
- [x] identidad cruda `nsc3`/`nsc4` de objetos e imágenes;
- [x] identidad entre repeticiones;
- [x] O0–O3, LLD y NSLink;
- [x] cero normalización, omitidos y problemas;
- [x] baseline completo acumulado verde.

La siguiente tanda es T46/SH44: determinismo entre entornos equivalentes, cubriendo rutas, espacios,
Unicode, locale, zona horaria, workers y orden del filesystem.
