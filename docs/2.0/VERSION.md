> **Aviso vigente sobre NSMeta:** El versionado de NSMeta ya no aplica. Las menciones de versiones, compatibilidad por versión y requisitos de versión de este documento son históricas y no constituyen un contrato vigente. No debe reintroducirse el versionado de NSMeta ni rechazarse su lectura o compilación por una versión; la aceptación se determina por el contenido y los contratos aplicables.

# Contrato de versión NativeSharp 2.0

| Superficie | Valor estable |
|---|---:|
| Compilador | `2.0.0+build.<UTC>.<id>` |
| Lenguaje | `2.0.0` |
| Stage interno | `70` |
| NSMETA | `1.20` |
| Native ABI | `1.0` |
| Runtime ABI | `1.0` |
| Runtime symbol set | `2` |
| MIR | `1.11` |

El canal `-dev` fue retirado por la promoción oficial. `+build` permanece para distinguir de forma
reproducible cada ejecutable construido sin declarar una versión de producto diferente.

---

# NativeSharp 2.0 version contract

The stable compiler identity is `2.0.0+build.<UTC>.<id>`, language identity is `2.0.0`, and the
internal stage is 70. The `-dev` channel was removed by official promotion. Build metadata remains
to distinguish exact binaries without declaring a different product release.
