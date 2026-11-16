> **Vigencia:** NativeSharp es un lenguaje único y acumulativo; el producto es 1.0. Las referencias históricas a ediciones o versiones de lenguaje y NSMeta que aparezcan a continuación no definen compatibilidad ni deben reintroducirse.

> **Aviso vigente sobre NSMeta:** El versionado de NSMeta ya no aplica. Las menciones de versiones, compatibilidad por versión y requisitos de versión de este documento son históricas y no constituyen un contrato vigente. No debe reintroducirse el versionado de NSMeta ni rechazarse su lectura o compilación por una versión; la aceptación se determina por el contenido y los contratos aplicables.

# Contrato de versión NativeSharp

| Superficie | Valor estable |
|---|---:|
| Compilador | `1.0.0+build.<UTC>.<id>` |
| Lenguaje | único y acumulativo; sin selector de versión |
| NSMeta | compatibilidad determinada por el contenido |
| Native ABI | `1.0` |
| Runtime ABI | `1.0` |
| Runtime symbol set | `2` |
| MIR | `1.11` |

`+build` distingue de forma reproducible cada ejecutable construido sin declarar una versión de
producto diferente. Las demás filas identifican contratos técnicos independientes del lenguaje y
de NSMeta.

---

# NativeSharp version contract

The compiler identity is `1.0.0+build.<UTC>.<id>`. The language is single and cumulative, with no
language-version selector. NSMeta compatibility is determined by content. Build metadata identifies
exact binaries without declaring a different product release.
