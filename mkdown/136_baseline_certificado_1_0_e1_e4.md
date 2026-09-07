# 136 — Baseline certificado de las megaentregas 1.0 E1–E4

**Fecha:** 2026-09-07  
**Gate:** G00 `20260907-112232-d023cfe0`  
**Estado:** `LANDED`  
**Certificado:** sí  
**Fallos:** 0

## Qué acaba de cerrarse

Las megaentregas E1–E4 ya no son únicamente cambios que compilan y pasan la matriz desde el árbol
de desarrollo. El compilador fue reconstruido, instalado y vuelto a probar bajo el gate externo con
el árbol inmóvil.

El baseline reúne cuatro contratos:

1. E1: objetos públicos completos consumibles desde bibliotecas precompiladas.
2. E2: templates públicos y especializaciones genéricas cruzando proyectos y productos.
3. E3: lifetime de delegates, closures y excepciones tipadas entre módulos.
4. E4: Native ABI 1.0, Runtime ABI 1.0 y NSMETA 1.8 como contrato verificable.

La consecuencia práctica es importante: esos cuatro bloques forman parte del `nsc.exe` instalado y
no dependen de una selección manual de binario ni de una construcción local distinta.

## Evidencia exacta

| Medida | Resultado |
|---|---|
| Run de G00 | `20260907-112232-d023cfe0` |
| Estado | `LANDED` |
| Certificación | `true` |
| Etapas | 11/11 |
| Casos registrados | 324 |
| Fallos | 0 |
| Archivos bajo huella | 1013 |
| Huella S0 | `9b3c39b5c215d24da73e6091b03728fad8f3dc9650ab26cd5cf7fe61329248ed` |
| Huella S1 | `9b3c39b5c215d24da73e6091b03728fad8f3dc9650ab26cd5cf7fe61329248ed` |
| SHA-256 construido | `dc56280ddf658bc798e9258d80facfeb72748516265cc3a316d4060317955e00` |
| SHA-256 instalado | `dc56280ddf658bc798e9258d80facfeb72748516265cc3a316d4060317955e00` |

La identidad del producto instalado es:

```text
nsc 0.9.0+build.20260907T172233Z.312a1b69a3ec28c2
NativeSharp 0.1, stage 57
```

## Cómo leer las dos igualdades del gate

`S0 == S1` demuestra que ninguna fuente incluida en el gate cambió mientras corría la
certificación. Evita atribuir un resultado verde a un árbol distinto del que comenzó la medición.

`built_nsc == installed_nsc` demuestra que el ejecutable probado y el entregado son exactamente el
mismo archivo. No basta con que ambos reporten la misma versión: la igualdad se verifica por
SHA-256.

Juntas, estas igualdades cierran los dos huecos que una prueba local no puede cerrar por sí sola:
inmovilidad del árbol e identidad física del producto instalado.

## Recorrido cubierto

Las 11 etapas de G00 abarcaron:

- construcción e instalación del compilador;
- construcción de la matriz para lld-link y NSLink;
- sincronización de bibliotecas;
- CTest para ambos enlazadores;
- fixtures para ambos enlazadores;
- bug triage para ambos enlazadores.

Los 324 casos incluyen las dos configuraciones de optimización previstas por el runner. Esto
certifica tanto la ruta de generación como la composición física de ejecutables y bibliotecas.

## Diferencia entre versión del producto y versión ABI

El producto continúa identificándose como `nsc 0.9.0`. E4, en cambio, declara Native ABI 1.0 y
Runtime ABI 1.0 dentro de NSMETA 1.8. No son números rivales:

- `0.9.0` nombra la madurez pública actual del compilador;
- ABI 1.0 nombra la forma binaria que ya se decidió congelar;
- NSMETA 1.8 nombra la revisión del formato que transporta ese contrato.

Congelar primero el contrato permite usar el tramo restante para someterlo a más corpus y mejorar
sus diagnósticos sin rediseñar silenciosamente el formato.

## Archivos documentales actualizados

- `mkdown/121_next_steps_for_the_1_0_glory.md`: sustituye el estado provisional por el baseline
  certificado.
- `mkdown/132_1_0_e1_objetos_precompilados_completos.md`: registra la integración certificada de
  E1.
- `mkdown/133_1_0_e2_genericos_entre_bibliotecas.md`: registra la integración certificada de E2.
- `mkdown/134_1_0_e3_lifetime_delegates_excepciones_entre_modulos.md`: añade el cierre instalado de
  E3.
- `mkdown/135_1_0_e4_congelacion_abi_y_nsmeta_1_8.md`: conserva el acta, hashes e identidad del
  producto que certifica E4.
- `mkdown/CONTENIDO.txt`: incorpora este documento al recorrido histórico recomendado.

## Qué siguió después de este baseline

E5 y E6 implementaron el siguiente tramo: incompatibilidades ABI con causa precisa y verificación
conjunta de `.nsmeta`, archivo COFF y DLL. Su implementación se documenta en 137 y 138. Es trabajo
posterior a este run de G00, por lo que no se atribuye retroactivamente al baseline E1–E4. El G00
posterior `20260907-131523-7eda857d` sí las instaló y certificó; su acta independiente es el
documento 139.

Cada cambio de ese tramo seguirá la regla didáctica establecida: problema observable, decisión,
funciones y archivos tocados, evidencia, resultado y deuda que permanezca abierta.
