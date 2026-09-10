# 1.0.1 — Contratos NSMETA transitivos / Transitive NSMETA contracts

## Estado / Status

**ES.** DLL-P4 reconstruye y verifica una API pública aunque sus tipos nominales
pertenezcan a otra NSMETA. La corrección cubre tanto
`--verify-library-artifacts` como la materialización fuente-cero del consumidor.

**EN.** DLL-P4 reconstructs and verifies a public API even when its nominal types
belong to another NSMETA. The fix covers both `--verify-library-artifacts` and
source-free consumer materialization.

## Repro canónico / Canonical reproduction

La cadena reducida es:

```text
ImportedBase.dll -> ImportedDerived.dll -> ImportedBaseConsumer.exe
```

`ImportedDerived.Derived : ImportedBase.Base` se verifica y se consume sin el
fuente del productor. El consumidor se compiló en O0 y O2 y ambos ejecutables
terminaron con código `42`.

## Barrido de contratos / Contract audit

La prueba integrada recorre todas las posiciones nominales que el contrato
público NSMETA materializa actualmente:

| Posición NSMETA / NSMETA position | Cobertura / Coverage |
|---|---|
| `baseTypeKey` | `Derived : Base` entre dos módulos |
| `interfaceTypeKeys` | interfaz derivada e implementación de interfaces externas |
| `fields[].typeKey` | campo público de `Payload` externo y su layout |
| `members[].declaredTypeKey` | retorno, evento y tipo declarado externos |
| `members[].parameters[].typeKey` | parámetro y constructor con `Payload` externo |
| `delegateReturnTypeKey` y parámetros | delegate externo y evento suscrito/ejecutado |
| `enumUnderlyingTypeKey` | enum externo con representación `byte` |
| dispatch de clase/base/interfaz | llamadas por `Derived`, `Base`, `IScore` e `IExtended` |
| constructor implícito | clase precompilada sin campos y cadena base fuente-cero |
| clausura runtime transitiva | el consumidor conserva Base y Derived como dependencias |

Los casos ya cerrados de DLL-P3/P4 complementan esta matriz para genéricos y
especializaciones, ownership y parámetros `ref`/`out`/`in`, métodos estáticos y
de instancia con tipos importados, excepciones, delegates/closures, destrucción
y RTTI. El verificador sigue rechazando declaraciones que NSMETA no promete
materializar; no se interpreta esa frontera como soporte implícito.

## Corrección / Fix

- la API generada publica los `using` necesarios para todos los tipos nominales
  de su contrato, no sólo para los tipos propiedad del módulo;
- bases, interfaces y delegates de eventos se emiten con el nombre simple que
  acepta el parser, conservando su identidad mediante el namespace importado;
- el verificador conoce tipos propios y externos en cada posición nominal antes
  de analizar la API reconstruida;
- una clase precompilada sin almacenamiento de instancia puede recuperar su
  constructor predeterminado implícito; una clase con estado sigue sin inventar
  inicializadores que NSMETA no transporta.

## Evidencia local / Local evidence

```text
Stage46NativeLibrariesTests + lld-link: 183/183 PASS
Stage46NativeLibrariesTests + NSLink:   183/183 PASS
Suites adyacentes:                        8/8 PASS
Repro canónico O0:                    exit 42
Repro canónico O2:                    exit 42
NativeForms.Core:                     NSMETA ABI válido, 1656 símbolos requeridos
```

Todo se ejecutó en `A:\NSC-DEVEL-BRANCHES\nsc-devel-dlls`. No se instaló
nada en `C:` y no se usó ni modificó `A:\nsc-devel`.
