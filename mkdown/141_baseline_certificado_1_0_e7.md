# 141 — Baseline certificado de 1.0-E7

**Fecha:** 2026-09-07  
**Gate:** G00  
**Run:** `20260907-152255-f7e1f291`  
**Estado:** `LANDED`  
**Certificado:** sí  
**Fallos:** 0

## Resultado

E7 dejó de ser una entrega verificada únicamente en los árboles de desarrollo. G00 reconstruyó el
compilador, instaló exactamente ese producto y ejecutó las rutas de lld-link y NSLink sin que el
árbol cambiara durante la corrida.

El baseline promueve tres piezas que ahora viajan juntas con NativeSharp:

- la especificación normativa candidata para 1.0;
- la gramática EBNF correspondiente;
- el mapa que conecta cada cláusula normativa con implementación y evidencia.

Stage 60 no se limita a comprobar que esos archivos existen. Su prueba contractual lee las 42
cláusulas, verifica que ninguna quede sin traza y comprueba que las rutas de implementación y de
prueba sigan resolviendo a archivos reales.

## Evidencia exacta

| Dato | Valor |
|---|---|
| Directorio de corrida | `A:\nsc-devel\_nscbuilder\history\20260907-152255-f7e1f291` |
| Inicio | `2026-09-07T15:22:55-06:00` |
| Fin registrado | `2026-09-07T15:33:39-06:00` |
| Duración | 644.164 s |
| Estado | `LANDED` |
| Certificado | `true` |
| Fallos | 0 |
| Etapas | 11 |
| Casos | 324 |
| Archivos bajo huella | 1014 |
| Huella S0 | `34bc19f5743a12f9882cbba1ce10534331ba791cce78f7f6dc1233c1a4178629` |
| Huella S1 | `34bc19f5743a12f9882cbba1ce10534331ba791cce78f7f6dc1233c1a4178629` |
| SHA-256 construido | `0d19c7d0f1b1d7a8f9e14af4691d23cf21af704a085e0b895cfa76edd61750a3` |
| SHA-256 instalado | `0d19c7d0f1b1d7a8f9e14af4691d23cf21af704a085e0b895cfa76edd61750a3` |

La identidad visible del producto instalado es:

```text
nsc 0.9.0+build.20260907T212256Z.4a0b84382d83359b (NativeSharp 0.1, stage 60)
build 4a0b84382d83359b
SHA-256 0D19C7D0F1B1D7A8F9E14AF4691D23CF21AF704A085E0B895CFA76EDD61750A3
```

## Qué demuestran las dos identidades

`S0 == S1` fija el código fuente examinado. Los 1014 archivos de la huella eran los mismos antes de
construir y después de terminar las pruebas; por eso la certificación no mezcla dos revisiones.

`built_nsc == installed_nsc` fija el producto. El ejecutable que G00 produjo y el que dejó bajo
`C:\native-sharp` tienen el mismo SHA-256; por eso los resultados no pertenecen a una instalación
anterior.

Ambas igualdades son necesarias. Una corrida verde con fuentes móviles o con otro compilador
instalado sería evidencia útil para depurar, pero no un baseline.

## Recorrido de las 11 etapas

G00 ejecutó:

1. construcción e instalación del compilador;
2. construcción de CTest para lld-link y NSLink;
3. sincronización de las bibliotecas instaladas;
4. CTest, fixtures y bug triage con lld-link;
5. CTest, fixtures y bug triage con NSLink;
6. certificación final de las huellas y del ejecutable.

Las 11 etapas terminaron con exit 0. El reporte agregado contiene 324 casos y ninguno aparece en
`problems`.

## Cómo se conecta con la evidencia local

El documento 140 registró primero 954 comprobaciones del contrato en cada árbol y una selección de
37 suites focales por enlazador. Esa evidencia respondió si la especificación era internamente
completa y si sus cláusulas seguían conectadas con el compilador.

El G00 responde una pregunta distinta y complementaria: si el producto reconstruido e instalado
conserva el comportamiento completo del baseline. Por eso no se sustituyen entre sí:

```text
Stage 60 focal   regla -> implementación -> prueba
G00 completo     fuente inmóvil -> binario instalado -> 324 casos
```

## Archivos que este baseline promueve

- `spec/NativeSharp_Language_Specification_1_0.md`: contrato normativo de las 42 cláusulas;
- `spec/NativeSharp_Grammar_1_0.ebnf`: gramática formal;
- `spec/NativeSharp_1_0_conformance.map`: trazabilidad verificable;
- `tests/unit/Stage60LanguageContractTests.cpp`: comprobación mecánica del contrato;
- `include/nativesharp/Basic/Version.h`: identidad Stage 60;
- `CMakeLists.txt`: instalación de `spec/`, registro de Stage 60 y validación de la versión visible;
- `mkdown/140_1_0_e7_especificacion_normativa_y_trazabilidad.md`: historia técnica de E7.

## Estado de cierre

E7 está implementada, reconstruida, instalada y certificada. El baseline vigente es el run
`20260907-152255-f7e1f291`; cualquier entrega posterior debe medirse contra él sin atribuirle
cambios que aún no contenía.
