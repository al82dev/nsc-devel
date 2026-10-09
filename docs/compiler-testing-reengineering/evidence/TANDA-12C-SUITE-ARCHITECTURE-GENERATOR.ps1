Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Documentation-only allocator for Phase 1, Tanda 12C. It translates the
# audited contracts and designed cases into future suite ownership. It does not
# create a runner, fixture, build tree, executable, or product artifact.
$docsRoot = Split-Path -Parent $PSScriptRoot
$inventoryRoot = Join-Path $docsRoot 'inventory'
$components = @(Import-Csv -LiteralPath (Join-Path $inventoryRoot 'components.csv'))
$contracts = @(Import-Csv -LiteralPath (Join-Path $inventoryRoot 'contracts.csv'))
$cases = @(Import-Csv -LiteralPath (Join-Path $inventoryRoot 'test-case-design.csv'))

if ($components.Count -ne 57) { throw "Expected 57 components, found $($components.Count)" }
if ($contracts.Count -ne 1309) { throw "Expected 1309 contracts, found $($contracts.Count)" }
if ($cases.Count -ne 10193) { throw "Expected 10193 designed cases, found $($cases.Count)" }
if (@($components | Group-Object component | Where-Object Count -ne 1).Count -ne 0) {
    throw 'Component identities are not unique'
}

$contractById = @{}
foreach ($contract in $contracts) {
    if ($contractById.ContainsKey($contract.contract_id)) { throw "Duplicate contract $($contract.contract_id)" }
    $contractById[$contract.contract_id] = $contract
}

function Slug([string]$value) {
    return ($value.ToLowerInvariant() -replace '[^a-z0-9]+','-').Trim('-')
}

function Resolve-Executor($case) {
    if ($case.layer -eq 'historical-weekly') { return 'legacy_historical_suite' }
    if ($case.component -like 'Host.*' -and $case.layer -in @('unit','component')) {
        return 'nsc_component_tests'
    }
    return 'nsc_suite_controller+nsc'
}

function Resolve-Profile($case) {
    if ($case.layer -eq 'historical-weekly' -or $case.frequency -eq 'WEEKLY') { return 'WEEKLY' }
    if ($case.frequency -eq 'NIGHTLY' -or $case.layer -in @('self-host','packaging','build-contract')) {
        return 'NIGHTLY'
    }
    return 'PR'
}

function Resolve-Family($case,[string]$executor) {
    $slug = Slug $case.component
    if ($executor -eq 'legacy_historical_suite') { return 'historical/frozen' }
    if ($executor -eq 'nsc_component_tests') { return "host/$slug" }
    if ($case.component -like 'SelfHost.*') { return "ns/selfhost/$slug" }
    if ($case.component -like 'Library.*' -or $case.component -eq 'SDK') { return "ns/library/$slug" }
    if ($case.component -in @('BuildSystem','Packaging')) { return "process/$slug" }
    return "ns/product/$slug"
}

function Resolve-Optimization([string]$executor,[string]$layer) {
    if ($executor -eq 'nsc_component_tests') { return @{supported='N/A';default='N/A'} }
    if ($executor -eq 'legacy_historical_suite') { return @{supported='FROZEN_CONTRACT';default='FROZEN_CONTRACT'} }
    if ($layer -in @('build-contract','packaging')) { return @{supported='N/A';default='N/A'} }
    return @{supported='O0;O1;O2;O3';default='O0'}
}

$allocation = [System.Collections.Generic.List[object]]::new()
foreach ($case in $cases) {
    if (-not $contractById.ContainsKey($case.contract_id)) { throw "Unknown contract $($case.contract_id)" }
    $executor = Resolve-Executor $case
    $profile = Resolve-Profile $case
    $optimization = Resolve-Optimization $executor $case.layer
    $authority = if ($executor -eq 'legacy_historical_suite') { 'FROZEN_REFERENCE' } else { 'NEW_SUITE' }
    $isolation = if ($executor -eq 'nsc_component_tests') { 'RESET_PER_CASE' } elseif ($executor -eq 'legacy_historical_suite') { 'LEGACY_OWNED' } else { 'FRESH_TEMP_PER_CASE' }
    $rationale = if ($executor -eq 'nsc_component_tests') {
        'Contrato host aislable dentro de un único binario modular; sin un ejecutable por prueba.'
    } elseif ($executor -eq 'legacy_historical_suite') {
        'La batería histórica permanece byte-identical y sólo actúa como respaldo semanal.'
    } else {
        'El oráculo exige el compilador construido o una frontera real de proceso/artefacto.'
    }
    $allocation.Add([pscustomobject][ordered]@{
        case_id = $case.case_id
        contract_id = $case.contract_id
        component = $case.component
        layer = $case.layer
        planned_executor = $executor
        module_or_fixture_family = Resolve-Family $case $executor
        default_profile = $profile
        supported_build_modes = if ($executor -eq 'legacy_historical_suite') { 'FROZEN_CONTRACT' } else { 'Debug;Release' }
        supported_optimizations = $optimization.supported
        default_optimization = $optimization.default
        isolation = $isolation
        authority = $authority
        rationale = $rationale
    })
}

$waveMap = @{
    'Host.Basic'=1; 'Host.Source'=1; 'Host.Target'=1; 'Host.Language'=1; 'Host.Diagnostics'=1
    'Host.Lexer'=2; 'Host.AST'=2; 'Host.Parser'=2; 'Host.Sema'=2
    'Host.HIR'=3; 'Host.MIR'=3; 'Host.CFG'=3; 'Host.Dominance'=3; 'Host.SSA'=3; 'Host.Passes'=3
    'Host.Compilation'=4; 'Host.Frontend'=4
    'Host.Interop'=5; 'Host.LLVM'=5; 'Host.Runtime'=5; 'Host.Linker'=5
    'Host.Driver'=6; 'Host.LanguageService'=6; 'Host.nsc'=6
    'Library.NativeSharp.Core'=7; 'Library.NativeSharp.Core.Collections.Generic'=7
    'Library.NativeSharp.Core.Diagnostics'=7; 'Library.NativeSharp.Core.IO'=7
    'Library.NativeSharp.Core.Threading'=7; 'Library.NativeSharp.LLVM.C'=7
    'Library.NativeSharp.Win32.Compiler'=7; 'Library.Provenance'=7; 'SDK'=7
    'SelfHost.Basic'=8; 'SelfHost.Bootstrap'=8; 'SelfHost.CodeGen'=8; 'SelfHost.Diagnostics'=8
    'SelfHost.Documentation'=8; 'SelfHost.Driver'=8; 'SelfHost.IR'=8; 'SelfHost.Lexer'=8
    'SelfHost.Parser'=8; 'SelfHost.Project'=8; 'SelfHost.Projects'=8; 'SelfHost.Sema'=8
    'SelfHost.Source'=8; 'SelfHost.Symbols'=8; 'SelfHost.Syntax'=8; 'SelfHost.Types'=8
    'SelfHost.Bootstrap.Infrastructure'=9; 'SelfHost.Contracts'=9
    'BuildSystem'=10; 'Packaging'=10
    'Tooling'=11; 'ThirdParty.Documentation'=11; 'ThirdParty.zlib'=11; 'ThirdParty.zstd'=11
}
$wavePurpose = @{
    1='Fundaciones host sin pipeline completo'
    2='Sintaxis y semántica frontal'
    3='Representaciones intermedias y optimización'
    4='Sesión, frontend y construcción de cuerpos'
    5='Backend, ABI, runtime y enlace'
    6='Producto, comandos y servicio de lenguaje'
    7='Bibliotecas distribuidas y SDK'
    8='Compilador self-host escrito en NativeSharp'
    9='Infraestructura y oráculos self-host'
    10='Construcción, instalación y empaquetado'
    11='Disposición externa; sin implementación de casos NSC'
}

$waves = [System.Collections.Generic.List[object]]::new()
foreach ($component in $components | Sort-Object component) {
    if (-not $waveMap.ContainsKey($component.component)) { throw "No implementation wave for $($component.component)" }
    $componentCases = @($allocation | Where-Object component -eq $component.component)
    $executors = @($componentCases | Select-Object -ExpandProperty planned_executor -Unique | Sort-Object)
    $profiles = @($componentCases | Select-Object -ExpandProperty default_profile -Unique | Sort-Object)
    $wave = [int]$waveMap[$component.component]
    $waves.Add([pscustomobject][ordered]@{
        wave = $wave
        purpose = $wavePurpose[$wave]
        role = $component.role
        component = $component.component
        files = $component.files
        contracts = @($contracts | Where-Object component -eq $component.component).Count
        cases = $componentCases.Count
        executors = $executors -join ';'
        profiles = $profiles -join ';'
        source_document = $component.document
        source_test_plan = 'test-plans/' + [IO.Path]::GetFileNameWithoutExtension($component.document) + '-TEST-PLAN.md'
        disposition = if ($component.reconstructibility -like 'NO APLICA*') { $component.reconstructibility } else { 'IMPLEMENTAR EN FASE 2' }
    })
}

$allocationPath = Join-Path $inventoryRoot 'suite-case-allocation.csv'
$wavesPath = Join-Path $inventoryRoot 'implementation-waves.csv'
$allocation | Export-Csv -LiteralPath $allocationPath -NoTypeInformation -Encoding utf8
$waves | Sort-Object wave,component | Export-Csv -LiteralPath $wavesPath -NoTypeInformation -Encoding utf8

$summary = [ordered]@{
    components = $components.Count
    contracts = $contracts.Count
    cases = $cases.Count
    executors = [ordered]@{}
    profiles = [ordered]@{}
    layers = [ordered]@{}
    implementation_waves = 11
    new_suite_cases = @($allocation | Where-Object authority -eq 'NEW_SUITE').Count
    frozen_reference_cases = @($allocation | Where-Object authority -eq 'FROZEN_REFERENCE').Count
}
foreach ($group in $allocation | Group-Object planned_executor | Sort-Object Name) { $summary.executors[$group.Name] = $group.Count }
foreach ($group in $allocation | Group-Object default_profile | Sort-Object Name) { $summary.profiles[$group.Name] = $group.Count }
foreach ($group in $allocation | Group-Object layer | Sort-Object Name) { $summary.layers[$group.Name] = $group.Count }
[pscustomobject]$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $inventoryRoot 'suite-architecture-summary.json') -Encoding utf8 -NoNewline

if (@($allocation | Group-Object case_id | Where-Object Count -ne 1).Count -ne 0) { throw 'Case allocation is not one-to-one' }
if (@($allocation | Where-Object { -not $_.planned_executor -or -not $_.default_profile }).Count -ne 0) { throw 'Unallocated case' }
if (@($waves | Group-Object component | Where-Object Count -ne 1).Count -ne 0) { throw 'Component wave allocation is not one-to-one' }

[pscustomobject]$summary | ConvertTo-Json -Depth 5
