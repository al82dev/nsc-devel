# Entrega 234 — Baseline verificable de migración 2.0 / Verifiable 2.0 migration baseline

## Objetivo / Objective

**ES.** Cerrar el lote 218–234 con un punto de comparación ejecutable para la futura migración de
1.x a 2.0. La entrega no cambia la versión del lenguaje a 2.0 ni autoriza rupturas: obliga a que una
ruptura futura nombre el contrato vigente, su migración y su matriz de compatibilidad.

**EN.** Close deliveries 218–234 with an executable comparison point for the future 1.x-to-2.0
migration. This delivery neither changes the language version to 2.0 nor authorizes breaks; it
requires a future break to name the current contract, its migration, and compatibility matrix.

## Archivos creados / Created files

### `include/nativesharp/Driver/MigrationBaseline.h`

Publica `kMigrationBaselineFormatVersion`, `currentMigrationBaseline`,
`writeMigrationBaseline` y `verifyMigrationBaseline`.

### `src/Driver/MigrationBaseline.cpp`

Es la autoridad única del formato determinista `NSMIGRATION 1`. Congela, en orden canónico:

- líneas fuente de compilador y lenguaje;
- major objetivo sin afirmar promoción;
- versión NSMETA;
- Native ABI, Runtime ABI y conjunto de símbolos del runtime;
- esquema MIR;
- ABI de mangling, objetos, delegates y excepciones;
- formatos del contrato del toolchain y política de compatibilidad;
- obligación de ledger de rupturas, notas de migración y matriz N−1/N.

El escritor crea directorios padre y conserva bytes canónicos LF. El verificador rechaza cabecera,
orden, ausencia, valor incompatible o campos adicionales; no permite que una herramienta acepte
silenciosamente una migración parcial.

### `tests/unit/Stage69MigrationBaselineTests.cpp`

Prueba el framing, las identidades congeladas, los tres gates de promoción, escritura exacta,
verificación positiva, ruptura ABI no clasificada, campos desconocidos y las tres órdenes públicas.

## Archivos modificados / Modified files

- `src/Driver/Driver.cpp`: agrega `--print-migration-baseline`,
  `--write-migration-baseline` y `--verify-migration-baseline`, con ayuda pública.
- `src/Driver/CompatibilityPolicy.cpp`: publica `migration_baseline=1` dentro de la política
  instalada.
- `CMakeLists.txt`: compila el módulo, genera el baseline con el `nsc` recién enlazado, lo incluye
  en el inventario, lo instala y registra su prueba dirigida.
- `src/Driver/InstallationVerifier.cpp`: exige y valida el artefacto instalado.
- `tests/unit/Stage63InstallationVerifierTests.cpp`: cubre instalación completa y baseline
  incompatible.
- `tests/unit/Stage66E9E10ReleaseContractTests.cpp`: exige que la política publique el nuevo
  formato.
- `docs/1.0.1/compiler/NativeSharp_Compiler_Contract_1_0_1.md`: define `NSC-COMPAT-003` y extiende
  el layout mínimo.
- `docs/1.0.1/compiler/NativeSharp_Compiler_1_0_1_conformance.map`: enlaza contrato,
  implementación y evidencia.
- `docs/1.0.1/ROADMAP_TO_2_0.md`: conecta el ledger de rupturas con el artefacto instalable.

## Decisiones de diseño / Design decisions

1. **Baseline, no manifiesto de 2.0.** `target_major=2` expresa el destino de comparación;
   `status=design-baseline` impide confundirlo con una versión implementada o certificada.
2. **Autoridad derivada del compilador.** Las versiones provienen de las mismas constantes que
   NSMETA, MIR, runtime y packaging. No hay números copiados por un script de distribución.
3. **Comparación estricta.** El verificador representa deliberadamente el compilador actual. Cuando
   cambie un contrato, el error nombra la primera diferencia y fuerza su clasificación antes de
   actualizar el baseline.
4. **Instalación autenticada.** El archivo entra en `NativeSharpInstallation.files` y después en el
   manifiesto SHA-256; no queda como documento opcional fuera de la distribución.
5. **Sin baseline intermedio.** Se agrega evidencia durante la entrega, pero sólo se reconstruye
   `nsc`. La batería completa se ejecuta una vez tras este commit, como cierre acumulativo pedido.

The artifact is a current-contract baseline, not a 2.0 declaration. Its values come from compiler
authorities, verification is intentionally strict, and installation authenticates it with the
existing package inventory. A future change must classify differences before updating the file.

## Criterios de aceptación / Acceptance criteria

- El compilador imprime, escribe y verifica bytes `NSMIGRATION 1` canónicos.
- El baseline enumera todas las fronteras necesarias para evaluar una ruptura 2.0.
- Una diferencia o campo extra falla con diagnóstico accionable.
- La instalación lo genera con el compilador construido, lo copia, lo autentica y lo verifica.
- La política de compatibilidad anuncia su versión.
- La documentación vigente no presenta 2.0 como ya implementado.

## Mensaje de commit / Commit message

### Título ES / Spanish title

`Establece el baseline verificable de migración a 2.0`

### Title EN / English title

`Establish the verifiable 2.0 migration baseline`

### Descripción ES / Spanish description

`Agrega el formato NSMIGRATION 1, sus órdenes públicas, generación e instalación autenticada, y congela las identidades 1.x que toda ruptura futura deberá clasificar mediante ledger, notas y matriz N−1/N.`

### Description EN / English description

`Add the NSMIGRATION 1 format, its public commands, authenticated generation and installation, and freeze the 1.x identities every future break must classify through a ledger, notes, and an N-1/N matrix.`
