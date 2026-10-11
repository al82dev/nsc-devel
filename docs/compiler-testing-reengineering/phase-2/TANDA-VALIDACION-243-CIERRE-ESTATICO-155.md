# Tanda de validación 243 — cierre estático del inventario de 155 casos

Fecha: 2026-10-10

## Estado

IMPLEMENTACIÓN RECONCILIADA. VALIDACIÓN DE EJECUCIÓN PENDIENTE.

## Autoridad y denominador

El inventario nace de los `156 FAIL` de
`20261010-181736-433daee9`, después de retirar exclusivamente
`TF-FRONT-192`, validado por separado en la Tanda 172. El denominador de este
cierre es por tanto exactamente `155` casos.

La clasificación inicial era:

| Clase inicial | Casos |
|---|---:|
| Defecto de prueba/oráculo | 125 |
| Defecto real de producto | 22 |
| Pendiente de evidencia | 2 |
| Consumidor derivado | 6 |
| **Total** | **155** |

## Resultado de la reconciliación

`TF-SSA-273` dejó de ser pendiente al demostrarse estáticamente que la
variante nominal duplicaba el local en el `else`; se corrigió como oráculo en
la Tanda 210. `TS-SEMA-111` sigue pendiente porque su recibo histórico sólo
conserva un predicado compuesto. El cierre queda:

| Estado final | Casos |
|---|---:|
| Prueba/oráculo implementado | 126 |
| Producto implementado | 22 |
| `BLOQUEADO_POR` productor HIR | 6 |
| `PENDIENTE_EVIDENCIA` | 1 |
| **Total** | **155** |

`148` significa implementación commiteada, no `PASS`: por orden del usuario
no se compiló ni ejecutó la batería durante este cierre.

## Inventario implementado — 148

### Host.Compilation — 11

`TF-COMPILATION-100`, `209`, `210`, `211`, `212`, `213`, `214`, `215`,
`216`, `334`, `381`.

Evidencia: Tandas 173–176; commits `74d950213`, `a86d38bf6`, `e61ace7a6` y
`e6a3de7ba`.

### Host.Dominance — 12

`TF-DOM-024`, `030`, `032`, `040`, `072`, `080`, `104`, `128`, `216`,
`224`, `232`, `256`.

Evidencia: Tandas 223–226; commits `e7db3d83c`, `ab3309e4b`, `970d0db70` y
`14781940e`.

### Host.Driver — 18

`TF-DRIVER-027`, `075`, `093`, `113`, `114`, `115`, `116`, `117`, `118`,
`119`, `120`, `361`, `362`, `363`, `365`, `366`, `367`, `368`.

Evidencia: Tandas 186–190; commits `14938ebf7`, `315a1281b`, `8b29c105a`,
`dee807819` y `fa2c7fe4d`.

### Host.HIR — 5

`TF-HIR-120`, `166`, `188`, `207`, `208`.

Evidencia: Tandas 193–195 y 227; commits `d37e27551`, `718876325`,
`4bba1f346` y `40f22fd83`.

### Host.Interop — 16

`TF-INTEROP-081` a `088` y `TF-INTEROP-137` a `144`.

Evidencia: Tandas 191–192; commits `37eedc93e` y `f45940d08`.

### Host.LanguageService — 14

`TF-LANGUAGE-SERVICE-125`, `139`, `140`, `145`, `146`, `149`, `150`, `151`,
`152`, `241`, `242`, `247`, `248`, `334`.

Evidencia: Tandas 177–181; commits `52dae0c58`, `77d45a24c`, `4a99e5a73`,
`b3d741b80` y `025f84939`.

### Host.MIR — 12 implementados

`TF-MIR-102`, `116`, `129`, `143`, `178`, `186`, `187`, `248`, `267`,
`271`, `293`, `296`.

Evidencia: Tandas 196–204; commits `f2ea85e6a`, `4fbf913b9`, `ed6fbda66`,
`aa5ceb23c`, `4587a636a`, `8e91aa8c2`, `5dc976157`, `0e5cf2a52` y
`1cc633ff6`.

### Host.Parser — 8

`TP-PAR-020`, `024`, `026`, `160`, `169`, `170`, `189`, `200`.

Evidencia: Tandas 182–185 y 229–232; commits `e32f4bf87`, `7907e5c90`,
`750d6d152`, `a84b4b94e`, `da37e64d9`, `ac45af936`, `956b911b4` y
`dcd8abda5`.

### Host.Passes — 14

`TF-PAS-273` a `280`, `TF-PAS-393` a `397` y `TF-PAS-399`.

Evidencia: Tandas 205–206; commits `f693064d4` y `b597c773a`.

### Host.Sema — 32 implementados

`TS-SEMA-100`, `116`, `118`, `123`, `125`, `127`, `128`, `132`, `135`,
`140`, `145`, `152`, `157`, `158`, `161`, `167`, `168`, `173`, `193`,
`204`, `208`, `214`, `216`, `219`, `220`, `222`, `237`, `238`, `240`,
`266`, `285`, `311`.

Evidencia: Tandas Sema 226–241; commits `d25e8e121`, `fb268930f`,
`de86084b0`, `deb2d655c`, `1d8304492`, `ac5eca10d`, `d7f505c24`,
`aa2f21256`, `e41765043`, `e5af85a2d`, `0e920fe25` y `b1e131a42`.

### Host.SSA — 5

`TF-SSA-134`, `198`, `268`, `273`, `294`.

Evidencia: Tandas 207–210; commits `6fe26cac4`, `20c655aee`, `e576e3f62` y
`f3741d07e`.

### SelfHost.Basic — 1

`TSH-00019`.

Evidencia: Tanda 211; commit `a57ce58b7`.

## Consumidores derivados — 6

`TF-MIR-152`, `183`, `195`, `198`, `200` y `270` permanecen
`BLOQUEADO_POR TF-HIR-120`. El productor fue implementado en `40f22fd83`, pero
por causalidad estos consumidores no se promueven a implementados ni a `PASS`
hasta que la corrida del usuario confirme el productor y los reevalúe.

## Pendiente de evidencia — 1

`TS-SEMA-111` permanece `PENDIENTE_EVIDENCIA`. La Tanda 242 y el commit
`2db99b3e1` documentan por qué el recibo disponible no permite elegir entre
símbolo ausente, tipo no completo o diagnóstico ausente.

## Integridad

| Componente | Total original | Implementado | Derivado | Pendiente |
|---|---:|---:|---:|---:|
| Host.Compilation | 11 | 11 | 0 | 0 |
| Host.Dominance | 12 | 12 | 0 | 0 |
| Host.Driver | 18 | 18 | 0 | 0 |
| Host.HIR | 5 | 5 | 0 | 0 |
| Host.Interop | 16 | 16 | 0 | 0 |
| Host.LanguageService | 14 | 14 | 0 | 0 |
| Host.MIR | 18 | 12 | 6 | 0 |
| Host.Parser | 8 | 8 | 0 | 0 |
| Host.Passes | 14 | 14 | 0 | 0 |
| Host.Sema | 33 | 32 | 0 | 1 |
| Host.SSA | 5 | 5 | 0 | 0 |
| SelfHost.Basic | 1 | 1 | 0 | 0 |
| **Total** | **155** | **148** | **6** | **1** |

No existen casos fuera de esas tres columnas finales. Este documento no
afirma resultados de ejecución posteriores a los commits.
