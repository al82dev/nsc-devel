Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Documentation-only repair discovered by the independent Phase 1 audit.
# It normalizes inventory references; it never edits or executes product/tests.
$docsRoot = Split-Path -Parent $PSScriptRoot
$repoRoot = Split-Path -Parent (Split-Path -Parent $docsRoot)
$inventoryRoot = Join-Path $docsRoot 'inventory'
$symbolsPath = Join-Path $inventoryRoot 'symbols.csv'
$contractsPath = Join-Path $inventoryRoot 'contracts.csv'
$coveragePath = Join-Path $inventoryRoot 'document-coverage.csv'
$productionPath = Join-Path $inventoryRoot 'production-files.csv'
$componentsPath = Join-Path $inventoryRoot 'components.csv'

$symbols = [Collections.Generic.List[object]]::new()
foreach ($row in Import-Csv -LiteralPath $symbolsPath) { $symbols.Add($row) }
$contracts = @(Import-Csv -LiteralPath $contractsPath)
$coverage = [Collections.Generic.List[object]]::new()
foreach ($row in Import-Csv -LiteralPath $coveragePath) { $coverage.Add($row) }
$production = @(Import-Csv -LiteralPath $productionPath)
$components = @(Import-Csv -LiteralPath $componentsPath)

$protectedHashes = [ordered]@{
    'docs/compiler-optimization/test-infrastructure/test-execution-parity.csv'='96c6e1564f973b36f4ab8cdd2b5df515e5128ee1345dd622024d15e9799f606d'
    'docs/compiler-optimization/test-infrastructure/test-execution-parity.json'='2dac516432a277fd4c0c2cd3a4868d348a6f626732f01243047919e32faa727a'
    'docs/compiler-optimization/test-infrastructure/test-execution-parity.md'='27802a8867f752fb6efd5b063264626d837e43cbea0a5362f5aa166ac4feaa4a'
    'tests/harness/nsc_test/suites.py'='407b534d216270b052aa35416d01d690219feba2b8c6d6438fec3634c5418b38'
    'tests/harness/run_nsc_tests.py'='e5ecf33859092b134b83f2665c659bc5691def40e3e31def39de9a4cf9946151'
    'tests/harness/tests/test_suite_adapter.py'='fef8fe5d953b0d2d3dbbf747d97719b8fc12f829d2cef0334a5025389db68dc0'
}
function Assert-Protected {
    foreach ($entry in $protectedHashes.GetEnumerator()) {
        $actual=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $repoRoot $entry.Key)).Hash.ToLowerInvariant()
        if($actual -ne $entry.Value){throw "Protected file changed: $($entry.Key)"}
    }
}
Assert-Protected

$productionByPath=@{};$productionById=@{}
foreach($row in $production){$productionByPath[$row.path]=$row;$productionById[$row.id]=$row}
$componentByName=@{};foreach($row in $components){$componentByName[$row.component]=$row}
$contractById=@{};foreach($row in $contracts){$contractById[$row.contract_id]=$row}

function Split-List([string]$value){return @($value -split ';'|Where-Object{$_})}
function Append-Unique([string]$value,[string]$item){return (@(@(Split-List $value)+@($item))|Select-Object -Unique)-join';'}
function Leaf([string]$qualified){return ($qualified -split '::')[-1]}
function File-Line([string]$path,[string]$pattern){
    $match=Select-String -LiteralPath (Join-Path $repoRoot $path) -Pattern $pattern|Select-Object -First 1
    if($null -eq $match){throw "Declaration not found in ${path}: $pattern"}
    return $match.LineNumber
}

# Resolve the sole inherited duplicate without renumbering the Driver symbol.
$duplicate=@($symbols|Where-Object symbol_id -eq 'SYM-005151')
if($duplicate.Count -eq 2){
    $languageService=$duplicate|Where-Object path -eq 'include/nativesharp/LanguageService/LanguageService.h'|Select-Object -First 1
    if($null -eq $languageService){throw 'LanguageService duplicate row not found'}
    $languageService.symbol_id='SYM-008402'
} elseif($duplicate.Count -eq 1) {
    $languageService=$symbols|Where-Object symbol_id -eq 'SYM-008402'|Where-Object path -eq 'include/nativesharp/LanguageService/LanguageService.h'|Select-Object -First 1
    if($null -eq $languageService){throw 'Repaired LanguageService symbol SYM-008402 not found'}
} else {
    throw "Expected inherited or repaired SYM-005151 state, found $($duplicate.Count) Driver rows"
}
$languageContract=$contractById['CF-LANGUAGE-SERVICE-010']
$languageContract.source_symbols=(Split-List $languageContract.source_symbols|ForEach-Object{if($_ -eq 'SYM-005151'){'SYM-008402'}else{$_}})-join';'
foreach($row in $coverage|Where-Object file_id -eq 'PF-000258'){$row.symbol_id=(Split-List $row.symbol_id|ForEach-Object{if($_ -eq 'SYM-005151'){'SYM-008402'}else{$_}})-join';'}

# Expand the three human range shorthands into machine-resolvable IDs.
$rangeExpansions=@{
    'CD-DIA-004..CD-DIA-008'=@('CD-DIA-004','CD-DIA-005','CD-DIA-006','CD-DIA-007','CD-DIA-008')
    'CD-DIA-009..CD-DIA-012'=@('CD-DIA-009','CD-DIA-010','CD-DIA-011','CD-DIA-012')
    'CL-LEX-004..CL-LEX-014'=@('CL-LEX-004','CL-LEX-005','CL-LEX-006','CL-LEX-007','CL-LEX-008','CL-LEX-009','CL-LEX-010','CL-LEX-011','CL-LEX-012','CL-LEX-013','CL-LEX-014')
}
foreach($row in $symbols){
    $expanded=[Collections.Generic.List[string]]::new()
    foreach($id in Split-List $row.contract_id){
        if($rangeExpansions.ContainsKey($id)){foreach($value in $rangeExpansions[$id]){$expanded.Add($value)}}else{$expanded.Add($id)}
    }
    $row.contract_id=(@($expanded)|Select-Object -Unique)-join';'
}

$headerSpecs=@(
    @{path='include/nativesharp/Driver/BreakingChangeLedger.h';functions=@('currentBreakingChangeLedger','writeBreakingChangeLedger','verifyBreakingChangeLedger');extras=@(@{kind='constant';name='nativesharp::driver::kBreakingChangeLedgerFormatVersion';pattern='kBreakingChangeLedgerFormatVersion'})},
    @{path='include/nativesharp/Driver/CompatibilityPolicy.h';functions=@('currentCompatibilityPolicy','writeCompatibilityPolicy','verifyCompatibilityPolicy','verifyInstalledCompatibilityCapabilities','checkCompilerCompatibility','currentCompatibilityMatrix','writeCompatibilityMatrix','verifyCompatibilityMatrix');extras=@(@{kind='constant';name='nativesharp::driver::kCompatibilityPolicyFormatVersion';pattern='kCompatibilityPolicyFormatVersion'},@{kind='constant';name='nativesharp::driver::kCompatibilityMatrixFormatVersion';pattern='kCompatibilityMatrixFormatVersion'})},
    @{path='include/nativesharp/Driver/DiagnosticBundle.h';functions=@('writeDiagnosticBundle');extras=@()},
    @{path='include/nativesharp/Driver/FinalBaselineContract.h';functions=@('currentFinalBaselineContract','writeFinalBaselineContract','verifyFinalBaselineContract');extras=@(@{kind='constant';name='nativesharp::driver::kFinalBaselineContractFormatVersion';pattern='kFinalBaselineContractFormatVersion'})},
    @{path='include/nativesharp/Driver/InstallationManifest.h';functions=@('writeInstallationManifest','verifyInstallationManifest');extras=@(@{kind='constant';name='nativesharp::driver::kInstallationFileListFormatVersion';pattern='kInstallationFileListFormatVersion'},@{kind='constant';name='nativesharp::driver::kInstallationManifestFormatVersion';pattern='kInstallationManifestFormatVersion'})},
    @{path='include/nativesharp/Driver/InstallationVerifier.h';functions=@('verifyInstallation');extras=@()},
    @{path='include/nativesharp/Driver/MigrationBaseline.h';functions=@('currentMigrationBaseline','writeMigrationBaseline','verifyMigrationBaseline');extras=@(@{kind='constant';name='nativesharp::driver::kMigrationBaselineFormatVersion';pattern='kMigrationBaselineFormatVersion'})},
    @{path='include/nativesharp/Driver/MigrationGuide.h';functions=@('currentMigrationGuide','writeMigrationGuide','verifyMigrationGuide');extras=@(@{kind='constant';name='nativesharp::driver::kMigrationGuideFormatVersion';pattern='kMigrationGuideFormatVersion'})},
    @{path='include/nativesharp/Driver/NumericContract.h';functions=@('numericContractManifest','numericContractFingerprintHex','writeNumericContract','verifyNumericContractFile');extras=@()},
    @{path='include/nativesharp/Driver/ProjectCommands.h';functions=@('isProjectCommand','runProjectCommand');extras=@(@{kind='type-alias';name='nativesharp::driver::DriverExecutor';pattern='using DriverExecutor'})},
    @{path='include/nativesharp/Driver/PromotionAct.h';functions=@('currentPromotionAct','writePromotionAct','verifyPromotionAct');extras=@(@{kind='constant';name='nativesharp::driver::kPromotionActFormatVersion';pattern='kPromotionActFormatVersion'})},
    @{path='include/nativesharp/Driver/ReleaseCandidateFreeze.h';functions=@('currentReleaseCandidateFreeze','writeReleaseCandidateFreeze','verifyReleaseCandidateFreeze');extras=@(@{kind='constant';name='nativesharp::driver::kReleaseCandidateFreezeFormatVersion';pattern='kReleaseCandidateFreezeFormatVersion'})},
    @{path='include/nativesharp/Driver/ToolchainContract.h';functions=@('currentToolchainContract','writeToolchainContract','verifyToolchainContract');extras=@(@{kind='constant';name='nativesharp::driver::kToolchainContractFormatVersion';pattern='kToolchainContractFormatVersion'})},
    @{path='include/nativesharp/Linker/WindowsRuntimeLibrary.h';functions=@('configuredWindowsRuntimeLibrary','resolveWindowsRuntimeLibrary','writeWindowsRuntimeLibrary','verifyWindowsRuntimeLibrary');extras=@(@{kind='constant';name='nativesharp::linker::kWindowsRuntimeLibraryFormatVersion';pattern='kWindowsRuntimeLibraryFormatVersion'},@{kind='constant';name='nativesharp::linker::kWindowsRuntimeLibraryFileName';pattern='kWindowsRuntimeLibraryFileName'})},
    @{path='include/nativesharp/LLVM/ProjectOopMirCacheIdentity.h';functions=@('projectOopMirCacheIdentity');extras=@(@{kind='forward-declaration';name='nativesharp::frontend::ProjectOopMirModel';pattern='class ProjectOopMirModel'})}
)

[int]$maxSymbol=($symbols|ForEach-Object{if($_.symbol_id-match'^SYM-(\d+)$'){[int]$matches[1]}}|Measure-Object -Maximum).Maximum
function New-SymbolId{$script:maxSymbol++;return ('SYM-{0:D6}' -f [int]$script:maxSymbol)}
[int]$addedSymbols=0

$touchedFiles=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach($spec in $headerSpecs){
    $file=$productionByPath[$spec.path]
    if($null -eq $file){throw "Header not in production inventory: $($spec.path)"}
    [void]$touchedFiles.Add($file.id)
    $matched=[Collections.Generic.List[object]]::new()
    foreach($functionName in $spec.functions){
        $candidates=@($symbols|Where-Object{(Leaf $_.qualified_name)-eq$functionName-and$_.component-eq$file.component-and$_.path-notlike'*.h'})
        if($candidates.Count-ne1){throw "$($spec.path): expected one implementation symbol for $functionName, found $($candidates.Count)"}
        $symbol=$candidates[0]
        $ids=Split-List $symbol.file_id;$paths=Split-List $symbol.path
        if($file.id-notin$ids){$symbol.file_id=(@($file.id)+$ids)-join';';$symbol.path=(@($file.path)+$paths)-join';'}
        $matched.Add($symbol)
    }
    $primaryContract=@($matched|ForEach-Object{Split-List $_.contract_id}|Select-Object -First 1)
    if($primaryContract.Count-ne1-or-not$contractById.ContainsKey($primaryContract[0])){throw "No contract for $($spec.path)"}
    foreach($extra in $spec.extras){
        $existing=@($symbols|Where-Object{$_.file_id-eq$file.id-and$_.qualified_name-eq$extra.name})
        if($existing.Count-gt1){throw "Duplicate repaired symbol: $($extra.name)"}
        if($existing.Count-eq1){
            $id=$existing[0].symbol_id
        } else {
            $id=New-SymbolId
            $row=[pscustomobject][ordered]@{
                symbol_id=$id;file_id=$file.id;path=$file.path;component=$file.component;kind=$extra.kind;qualified_name=$extra.name
                line=File-Line $file.path $extra.pattern;visibility='public'
                responsibility="Declare the public $($extra.kind) $($extra.name)"
                inputs='declaration-time constant or type contract';preconditions='header is consumed under its owning component'
                outputs='stable public declaration';invariants='identity, type and value remain aligned with the owning contract'
                errors='compile-time mismatch is a contract violation';side_effects='none';dependencies='owning header dependencies'
                observability='independent compilation plus exact declaration/value inspection';test_layer='unit';contract_id=$primaryContract[0]
                state='CONSOLIDADO';document=$componentByName[$file.component].document;section="contract $($primaryContract[0])"
            }
            $symbols.Add($row)
            $addedSymbols++
        }
        $contract=$contractById[$primaryContract[0]]
        $contract.source_symbols=Append-Unique $contract.source_symbols $id
    }
}

# Project and architecture documents are artifacts with contracts, not missing code declarations.
$artifactSpecs=@(
    @{path='selfhost/nsc/NativeSharp.Compiler.nsproj';kind='project-manifest';name='NativeSharpCompilerNext project manifest';contract='CSH-PRO-001'},
    @{path='selfhost/nsc/README.md';kind='architecture-document';name='NativeSharpCompilerNext architecture declaration';contract='CSH-DOC-001'}
)
foreach($spec in $artifactSpecs){
    $file=$productionByPath[$spec.path];if($null-eq$file){throw "Artifact missing: $($spec.path)"};[void]$touchedFiles.Add($file.id)
    $existing=@($symbols|Where-Object{$_.file_id-eq$file.id-and$_.qualified_name-eq$spec.name})
    if($existing.Count-gt1){throw "Duplicate repaired artifact symbol: $($spec.name)"}
    if($existing.Count-eq1){
        $id=$existing[0].symbol_id
    } else {
        $id=New-SymbolId
        $symbols.Add([pscustomobject][ordered]@{
            symbol_id=$id;file_id=$file.id;path=$file.path;component=$file.component;kind=$spec.kind;qualified_name=$spec.name;line=1;visibility='project'
            responsibility='Declare the self-host project or architecture contract';inputs='versioned repository artifact';preconditions='artifact is present in the frozen source tree'
            outputs='project composition or normative architecture';invariants='content remains aligned with the owning self-host contracts'
            errors='missing or contradictory declaration invalidates reconstruction';side_effects='none';dependencies='self-host source inventory'
            observability='exact parsed project/document structure';test_layer='component';contract_id=$spec.contract;state='CONSOLIDADO'
            document=$componentByName[$file.component].document;section="contract $($spec.contract)"
        })
        $addedSymbols++
    }
    $contract=$contractById[$spec.contract];$contract.source_symbols=Append-Unique $contract.source_symbols $id
}

# Complete only missing reverse relations. Existing logical subcontracts retain
# their authored source paths/names; this does not infer new ownership.
foreach($contract in $contracts|Where-Object{[string]::IsNullOrWhiteSpace($_.source_symbols)}){
    $owned=@($symbols|Where-Object{$contract.contract_id-in(Split-List $_.contract_id)}|ForEach-Object symbol_id|Select-Object -Unique)
    if($owned.Count-eq0){throw "Contract without resolvable source symbol: $($contract.contract_id)"}
    $contract.source_symbols=$owned-join';'
}

# Repair missing separators produced by the inherited scalar/list ambiguity.
# Valid source paths/names preceding the first symbol are preserved verbatim.
foreach($contract in $contracts){
    $normalized=[Collections.Generic.List[string]]::new()
    foreach($token in @(Split-List $contract.source_symbols)){
        $matches=[regex]::Matches($token,'SYM-\d{6}')
        if($matches.Count-eq0){$normalized.Add($token);continue}
        $prefix=$token.Substring(0,$matches[0].Index)
        if(-not[string]::IsNullOrWhiteSpace($prefix)){$normalized.Add($prefix)}
        foreach($match in $matches){$normalized.Add($match.Value)}
    }
    $contract.source_symbols=(@($normalized)|Select-Object -Unique)-join';'
}

# Rebuild coverage for every previously symbol-less primary file.
foreach($fileId in $touchedFiles){
    $file=$productionById[$fileId]
    $owned=@($symbols|Where-Object{$fileId-in(Split-List $_.file_id)})
    $symbolIds=($owned.symbol_id|Select-Object -Unique)-join';'
    $contractIds=@($owned|ForEach-Object{Split-List $_.contract_id}|Select-Object -Unique)
    $rows=@($coverage|Where-Object file_id -eq $fileId)
    if($rows.Count-eq0){
        $row=[pscustomobject][ordered]@{file_id=$file.id;path=$file.path;component=$file.component;file_state='AUDITADO';symbol_id=$symbolIds;contract_id=$contractIds-join';';document=$componentByName[$file.component].document;section='Tanda 13 physical trace';trace_state='TRAZADO';historical_state='CONTRASTADO';current_state='IMPLEMENTACION VIGENTE';notes='independent physical declaration reconciliation'}
        $coverage.Add($row);$rows=@($row)
    }
    foreach($row in $rows){$row.file_state='AUDITADO';$row.symbol_id=$symbolIds;$row.contract_id=$contractIds-join';';$row.document=$componentByName[$file.component].document;$row.section='Tanda 13 physical trace';$row.trace_state='TRAZADO';$row.historical_state='CONTRASTADO';$row.current_state='IMPLEMENTACION VIGENTE';$row.notes='independent physical declaration reconciliation'}
}

# Remove only superseded empty placeholders where a traced row for the same file exists.
$cleanCoverage=[Collections.Generic.List[object]]::new()
foreach($row in $coverage){
    $hasTraced=@($coverage|Where-Object{$_.file_id-eq$row.file_id-and$_.trace_state-eq'TRAZADO'}).Count-gt0
    if($row.file_state-eq'NO LEIDO'-and$row.trace_state-eq'NO TRAZADO'-and$hasTraced){continue}
    $cleanCoverage.Add($row)
}

$symbols|Export-Csv -LiteralPath $symbolsPath -NoTypeInformation -Encoding utf8
$contracts|Export-Csv -LiteralPath $contractsPath -NoTypeInformation -Encoding utf8
$cleanCoverage|Export-Csv -LiteralPath $coveragePath -NoTypeInformation -Encoding utf8

# The JSON mirror must represent the same current documentary state as CSV.
$production|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $inventoryRoot 'production-files.json') -Encoding utf8 -NoNewline

# Rewrite the summary after every canonical artifact above is final.
$cases=@(Import-Csv -LiteralPath (Join-Path $inventoryRoot 'test-case-design.csv'))
$historical=@(Import-Csv -LiteralPath (Join-Path $inventoryRoot 'historical-tests-freeze.csv'))
$transversal=@(Import-Csv -LiteralPath (Join-Path $inventoryRoot 'transversal-contracts.csv'))
$errors=@(Import-Csv -LiteralPath (Join-Path $inventoryRoot 'error-taxonomy.csv'))
$machines=@(Import-Csv -LiteralPath (Join-Path $inventoryRoot 'state-machines.csv'))
$machineMap=@(Import-Csv -LiteralPath (Join-Path $inventoryRoot 'state-machine-contract-map.csv'))
$summary=[ordered]@{
    source_revision=($production.revision|Select-Object -Unique)-join';'
    documentation_revision=(git -C $repoRoot rev-parse HEAD).Trim()
    production_files=$production.Count
    production_lines=($production|Measure-Object lines -Sum).Sum
    components=$components.Count
    symbols=$symbols.Count
    contracts=$contracts.Count
    test_cases=$cases.Count
    historical_test_files=$historical.Count
    transversal_contracts=$transversal.Count
    error_taxonomy_rows=$errors.Count
    state_machines=$machines.Count
    state_machine_contract_mappings=$machineMap.Count
    production_csv_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $productionPath).Hash.ToLowerInvariant()
    production_json_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $inventoryRoot 'production-files.json')).Hash.ToLowerInvariant()
    components_csv_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $componentsPath).Hash.ToLowerInvariant()
    document_coverage_csv_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $coveragePath).Hash.ToLowerInvariant()
    symbols_csv_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $symbolsPath).Hash.ToLowerInvariant()
    contracts_csv_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $contractsPath).Hash.ToLowerInvariant()
    test_case_design_csv_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $inventoryRoot 'test-case-design.csv')).Hash.ToLowerInvariant()
    historical_tests_csv_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $inventoryRoot 'historical-tests-freeze.csv')).Hash.ToLowerInvariant()
    suite_case_allocation_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $inventoryRoot 'suite-case-allocation.csv')).Hash.ToLowerInvariant()
    implementation_waves_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $inventoryRoot 'implementation-waves.csv')).Hash.ToLowerInvariant()
}
[pscustomobject]$summary|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $inventoryRoot 'manifest-summary.json') -Encoding utf8 -NoNewline

Assert-Protected
[pscustomobject]@{symbols=$symbols.Count;new_symbols=$addedSymbols;coverage_rows=$cleanCoverage.Count;empty_contract_sources=@($contracts|Where-Object{[string]::IsNullOrWhiteSpace($_.source_symbols)}).Count;duplicate_symbol_ids=@($symbols|Group-Object symbol_id|Where-Object{$_.Count-ne1}).Count}|ConvertTo-Json
