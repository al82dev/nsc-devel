# Evidencia — Tanda 02, grafo de productores y consumidores

## Gates mecánicos

| Gate | Resultado |
|---|---:|
| Targets internos inventariados | 12 |
| Fuentes host versionadas | 242 |
| Fuentes host asociadas a target | 242 |
| Fuentes host faltantes | 0 |
| Fuentes host extra | 0 |
| Fuentes self-host | 27 |
| Proyectos NativeSharp | 8 |
| Fuentes NativeSharp de proyectos | 119 |
| Fuentes NativeSharp sin proyecto | 0 |
| Dependencias de target registradas | 23 |
| Dependencias externas host | 6 |
| Ocurrencias de includes internos | 1,431 |
| Aristas conceptuales distintas | 112 |
| Componentes host | 24 |
| Grupos fuertemente conectados | 17 |
| Grupos cíclicos | 1 |

## Hechos comprobados

- Todo `.cpp` versionado bajo `src/**` aparece exactamente en un target de producto CMake.
- Toda fuente `.ns` bajo `lib/**` o `selfhost/nsc/**` aparece exactamente en un proyecto inventariado.
- El grafo físico de targets de producto es acíclico.
- El grafo conceptual de includes contiene un grupo conectado de ocho carpetas.
- El host exige NSLink al configurar, LLVM 23 y, en Windows estático, el runtime x64 fijado de Visual Studio
  2026; DIA es dependencia exclusiva de construcción del host.
- El self-host depende de cuatro proyectos NativeSharp y de una imagen productora anterior válida.

## Inferencia controlada

`INFERENCIA`: una suite rápida no debe usar como unidad de aislamiento todas las carpetas conceptuales. En el
grupo conectado, el límite estable para fixture puede ser el target físico o un contrato explícito más pequeño.
La decisión concreta queda pendiente de la auditoría de símbolos; esta tanda no implementa runners ni tests.

## Validación no ejecutada

No se configuró CMake, no se compiló ningún target y no se ejecutó ningún artefacto. Los resultados de esta
tanda son exclusivamente estáticos.
