Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Independent Phase 1 verifier. It re-enumerates the source universe from Git
# and validates the published inventories without invoking any earlier generator.
$docsRoot = Split-Path -Parent $PSScriptRoot
$repoRoot = Split-Path -Parent (Split-Path -Parent $docsRoot)
$inventoryRoot = Join-Path $docsRoot 'inventory'
$issues = [Collections.Generic.List[object]]::new()
$documentExistence = @{}

function Add-Issue([string]$code,[string]$subject,[string]$detail) {
    $issues.Add([pscustomobject][ordered]@{code=$code;subject=$subject;detail=$detail})
}
function Split-List([string]$value) {
    return @($value -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
}
function Read-Csv([string]$name) {
    return @(Import-Csv -LiteralPath (Join-Path $inventoryRoot $name))
}
function Assert-Unique($rows,[string]$property,[string]$code) {
    foreach($group in $rows | Group-Object -Property $property | Where-Object { $_.Count -ne 1 }) {
        Add-Issue $code $group.Name "count=$($group.Count)"
    }
}
function Test-Doc([string]$relative,[string]$subject) {
    if([string]::IsNullOrWhiteSpace($relative)) {
        Add-Issue 'DOC-MISSING' $subject $relative
        return
    }
    if(-not$documentExistence.ContainsKey($relative)) {
        $documentExistence[$relative]=Test-Path -LiteralPath (Join-Path $docsRoot $relative)
    }
    if(-not$documentExistence[$relative]) {
        Add-Issue 'DOC-MISSING' $subject $relative
    }
}
function Lower-Hash([string]$path) {
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
}

$protectedHashes = [ordered]@{
    'docs/compiler-optimization/test-infrastructure/test-execution-parity.csv'='96c6e1564f973b36f4ab8cdd2b5df515e5128ee1345dd622024d15e9799f606d'
    'docs/compiler-optimization/test-infrastructure/test-execution-parity.json'='2dac516432a277fd4c0c2cd3a4868d348a6f626732f01243047919e32faa727a'
    'docs/compiler-optimization/test-infrastructure/test-execution-parity.md'='27802a8867f752fb6efd5b063264626d837e43cbea0a5362f5aa166ac4feaa4a'
    'tests/harness/nsc_test/suites.py'='407b534d216270b052aa35416d01d690219feba2b8c6d6438fec3634c5418b38'
    'tests/harness/run_nsc_tests.py'='e5ecf33859092b134b83f2665c659bc5691def40e3e31def39de9a4cf9946151'
    'tests/harness/tests/test_suite_adapter.py'='fef8fe5d953b0d2d3dbbf747d97719b8fc12f829d2cef0334a5025389db68dc0'
}
foreach($entry in $protectedHashes.GetEnumerator()) {
    $actual=Lower-Hash (Join-Path $repoRoot $entry.Key)
    if($actual-ne$entry.Value){Add-Issue 'PROTECTED-HASH' $entry.Key "expected=$($entry.Value);actual=$actual"}
}

$production=Read-Csv 'production-files.csv'
$components=Read-Csv 'components.csv'
$symbols=Read-Csv 'symbols.csv'
$contracts=Read-Csv 'contracts.csv'
$cases=Read-Csv 'test-case-design.csv'
$coverage=Read-Csv 'document-coverage.csv'
$allocation=Read-Csv 'suite-case-allocation.csv'
$waves=Read-Csv 'implementation-waves.csv'
$historical=Read-Csv 'historical-tests-freeze.csv'
$selfhostAudit=Read-Csv 'selfhost-file-audit.csv'
$remainingAudit=Read-Csv 'remaining-surface-file-audit.csv'

# Re-enumerate the production/support universe from versioned paths only.
$gitPaths=@(& git -C $repoRoot ls-files -- src include/nativesharp selfhost/nsc selfhost/bootstrap selfhost/contracts lib sdk cmake tools third_party CMakeLists.txt deploy.bat deploy.py package_native_sharp.py)
if($LASTEXITCODE-ne0){throw 'git ls-files failed'}
$gitPaths=@($gitPaths|ForEach-Object{$_.Replace('\','/')}|Select-Object -Unique)
[Array]::Sort($gitPaths,[StringComparer]::OrdinalIgnoreCase)

$reenumerated=[Collections.Generic.List[object]]::new()
for($index=0;$index-lt$gitPaths.Count;$index++) {
    $path=$gitPaths[$index]
    $absolute=Join-Path $repoRoot $path
    $reenumerated.Add([pscustomobject][ordered]@{
        expected_id=('PF-{0:D6}' -f ($index+1))
        path=$path
        bytes=(Get-Item -LiteralPath $absolute).Length
        sha256=Lower-Hash $absolute
    })
}
$reenumerated|Export-Csv -LiteralPath (Join-Path $inventoryRoot 'independent-file-reenumeration.csv') -NoTypeInformation -Encoding utf8

Assert-Unique $production 'id' 'PRODUCTION-DUPLICATE-ID'
Assert-Unique $production 'path' 'PRODUCTION-DUPLICATE-PATH'
$productionById=@{};$productionByPath=@{}
foreach($row in $production){$productionById[$row.id]=$row;$productionByPath[$row.path]=$row}

foreach($item in $reenumerated) {
    if(-not$productionByPath.ContainsKey($item.path)){Add-Issue 'PRODUCTION-MISSING' $item.path 'present in independent Git enumeration only';continue}
    $row=$productionByPath[$item.path]
    if($row.id-ne$item.expected_id){Add-Issue 'PRODUCTION-ID-ORDER' $item.path "expected=$($item.expected_id);actual=$($row.id)"}
    if([int64]$row.bytes-ne[int64]$item.bytes){Add-Issue 'PRODUCTION-BYTES' $item.path "expected=$($item.bytes);actual=$($row.bytes)"}
    if($row.sha256-ne$item.sha256){Add-Issue 'PRODUCTION-HASH' $item.path "expected=$($item.sha256);actual=$($row.sha256)"}
}
foreach($row in $production){if($row.path-notin$gitPaths){Add-Issue 'PRODUCTION-EXTRA' $row.path 'present in inventory only'}}

# Verify that the JSON mirror is property-for-property equal to the canonical CSV.
$productionJson=@(Get-Content -LiteralPath (Join-Path $inventoryRoot 'production-files.json') -Raw|ConvertFrom-Json)
if($productionJson.Count-ne$production.Count){Add-Issue 'PRODUCTION-JSON-COUNT' 'production-files.json' "csv=$($production.Count);json=$($productionJson.Count)"}
$jsonById=@{};foreach($row in $productionJson){$jsonById[$row.id]=$row}
$productionColumns=@('id','path','role','component','language','bytes','lines','sha256','revision','state','evidence','section','notes')
foreach($row in $production){
    if(-not$jsonById.ContainsKey($row.id)){Add-Issue 'PRODUCTION-JSON-MISSING' $row.id $row.path;continue}
    $json=$jsonById[$row.id]
    foreach($column in $productionColumns){if([string]$row.$column-ne[string]$json.$column){Add-Issue 'PRODUCTION-JSON-DIFF' "$($row.id).$column" "csv=$($row.$column);json=$($json.$column)"}}
}

# Components must partition the 974 production files exactly.
Assert-Unique $components 'component' 'COMPONENT-DUPLICATE'
$componentByName=@{};foreach($row in $components){$componentByName[$row.component]=$row;Test-Doc $row.document $row.component;Test-Doc $row.audit_report "$($row.component) audit"}
foreach($group in $production|Group-Object component){
    if(-not$componentByName.ContainsKey($group.Name)){Add-Issue 'COMPONENT-MISSING' $group.Name "files=$($group.Count)";continue}
    $component=$componentByName[$group.Name]
    $lines=($group.Group|Measure-Object -Property lines -Sum).Sum
    if([int]$component.files-ne$group.Count){Add-Issue 'COMPONENT-FILE-COUNT' $group.Name "expected=$($group.Count);actual=$($component.files)"}
    if([int64]$component.lines-ne[int64]$lines){Add-Issue 'COMPONENT-LINE-COUNT' $group.Name "expected=$lines;actual=$($component.lines)"}
}
foreach($component in $components){if($component.component-notin$production.component){Add-Issue 'COMPONENT-EXTRA' $component.component 'no production files'}}

# Symbols: unique IDs, aligned multi-file references, valid contracts and explicit exclusions.
Assert-Unique $symbols 'symbol_id' 'SYMBOL-DUPLICATE-ID'
$symbolById=@{};$symbolByName=@{}
foreach($row in $symbols){
    $symbolById[$row.symbol_id]=$row
    if(-not$symbolByName.ContainsKey($row.qualified_name)){$symbolByName[$row.qualified_name]=[Collections.Generic.List[object]]::new()}
    $symbolByName[$row.qualified_name].Add($row)
}
$remainingDisposition=@{};foreach($row in $remainingAudit){$remainingDisposition[$row.file_id]=$row.disposition}
$symbolFileCounts=@{}
$requiredSymbolFields=@('symbol_id','file_id','path','component','kind','qualified_name','line','responsibility','test_layer','state','document')
foreach($row in $symbols){
    foreach($field in $requiredSymbolFields){if([string]::IsNullOrWhiteSpace([string]$row.$field)){Add-Issue 'SYMBOL-EMPTY-FIELD' "$($row.symbol_id).$field" $row.qualified_name}}
    $fileIds=@(Split-List $row.file_id);$paths=@(Split-List $row.path)
    if($fileIds.Count-ne$paths.Count){Add-Issue 'SYMBOL-FILE-ALIGNMENT' $row.symbol_id "file_ids=$($fileIds.Count);paths=$($paths.Count)"}
    for($i=0;$i-lt[Math]::Min($fileIds.Count,$paths.Count);$i++){
        if(-not$productionById.ContainsKey($fileIds[$i])){Add-Issue 'SYMBOL-UNKNOWN-FILE' $row.symbol_id $fileIds[$i];continue}
        if($productionById[$fileIds[$i]].path-ne$paths[$i]){Add-Issue 'SYMBOL-PATH-DIFF' $row.symbol_id "$($fileIds[$i]):$($paths[$i])"}
        if(-not$symbolFileCounts.ContainsKey($fileIds[$i])){$symbolFileCounts[$fileIds[$i]]=0}
        $symbolFileCounts[$fileIds[$i]]++
    }
    $contractIds=@(Split-List $row.contract_id)
    if($contractIds.Count-eq0){
        $allowed=$true
        foreach($fileId in $fileIds){if(-not$remainingDisposition.ContainsKey($fileId)-or$remainingDisposition[$fileId]-notin@('UPSTREAM','EXCLUIDA')){$allowed=$false}}
        if(-not$allowed){Add-Issue 'SYMBOL-WITHOUT-CONTRACT' $row.symbol_id $row.qualified_name}
    } else {
        foreach($contractId in $contractIds){if($contractId-notin$contracts.contract_id){Add-Issue 'SYMBOL-UNKNOWN-CONTRACT' $row.symbol_id $contractId}}
    }
    Test-Doc $row.document $row.symbol_id
}
foreach($file in $production|Where-Object role -eq 'PRIMARY_COMPILER'){
    $found=if($symbolFileCounts.ContainsKey($file.id)){$symbolFileCounts[$file.id]}else{0}
    if($found-eq0){Add-Issue 'PRIMARY-FILE-WITHOUT-SYMBOL' $file.id $file.path}
}

# Contracts: complete stable oracles, resolvable source links, and at least one designed case.
Assert-Unique $contracts 'contract_id' 'CONTRACT-DUPLICATE-ID'
$contractById=@{};foreach($row in $contracts){$contractById[$row.contract_id]=$row}
$contractComponentCounts=@{};foreach($group in $contracts|Group-Object component){$contractComponentCounts[$group.Name]=$group.Count}
$requiredContractFields=@('contract_id','component','name','source_symbols','inputs','preconditions','outputs','postconditions','invariants','errors','side_effects','dependencies','observable_oracle','test_layer','status','document','section')
foreach($row in $contracts){
    foreach($field in $requiredContractFields){if([string]::IsNullOrWhiteSpace([string]$row.$field)){Add-Issue 'CONTRACT-EMPTY-FIELD' "$($row.contract_id).$field" $row.name}}
    if(-not$componentByName.ContainsKey($row.component)){Add-Issue 'CONTRACT-UNKNOWN-COMPONENT' $row.contract_id $row.component}
    foreach($token in Split-List $row.source_symbols){
        $resolved=$symbolById.ContainsKey($token)-or$symbolByName.ContainsKey($token)-or$productionByPath.ContainsKey($token)
        if(-not$resolved){Add-Issue 'CONTRACT-UNRESOLVED-SOURCE' $row.contract_id $token}
    }
    Test-Doc $row.document $row.contract_id
}

# Designed cases and suite allocation must form exact 1:1 maps.
Assert-Unique $cases 'case_id' 'CASE-DUPLICATE-ID'
$caseById=@{};foreach($row in $cases){$caseById[$row.case_id]=$row}
$caseContractIds=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$caseComponentCounts=@{}
$requiredCaseFields=@('case_id','contract_id','component','objective','input_class','setup','input','expected','dependencies','layer','frequency','status','document','section')
foreach($row in $cases){
    foreach($field in $requiredCaseFields){if([string]::IsNullOrWhiteSpace([string]$row.$field)){Add-Issue 'CASE-EMPTY-FIELD' "$($row.case_id).$field" $row.objective}}
    if(-not$contractById.ContainsKey($row.contract_id)){Add-Issue 'CASE-UNKNOWN-CONTRACT' $row.case_id $row.contract_id}
    if(-not$componentByName.ContainsKey($row.component)){Add-Issue 'CASE-UNKNOWN-COMPONENT' $row.case_id $row.component}
    [void]$caseContractIds.Add($row.contract_id)
    if(-not$caseComponentCounts.ContainsKey($row.component)){$caseComponentCounts[$row.component]=0};$caseComponentCounts[$row.component]++
    Test-Doc $row.document $row.case_id
}
foreach($contract in $contracts){if(-not$caseContractIds.Contains($contract.contract_id)){Add-Issue 'CONTRACT-WITHOUT-CASE' $contract.contract_id $contract.name}}

Assert-Unique $allocation 'case_id' 'ALLOCATION-DUPLICATE-CASE'
$allocationById=@{};foreach($row in $allocation){$allocationById[$row.case_id]=$row}
foreach($case in $cases){
    if(-not$allocationById.ContainsKey($case.case_id)){Add-Issue 'ALLOCATION-MISSING' $case.case_id $case.contract_id;continue}
    $allocated=$allocationById[$case.case_id]
    foreach($field in @('contract_id','component','layer')){if($allocated.$field-ne$case.$field){Add-Issue 'ALLOCATION-DIFF' "$($case.case_id).$field" "case=$($case.$field);allocation=$($allocated.$field)"}}
}
foreach($row in $allocation){if(-not$caseById.ContainsKey($row.case_id)){Add-Issue 'ALLOCATION-EXTRA' $row.case_id $row.contract_id}}

Assert-Unique $waves 'component' 'WAVE-DUPLICATE-COMPONENT'
foreach($component in $components){
    $wave=@($waves|Where-Object component -eq $component.component)
    if($wave.Count-ne1){Add-Issue 'WAVE-COMPONENT-CARDINALITY' $component.component "count=$($wave.Count)";continue}
    $expectedContracts=if($contractComponentCounts.ContainsKey($component.component)){$contractComponentCounts[$component.component]}else{0}
    $expectedCases=if($caseComponentCounts.ContainsKey($component.component)){$caseComponentCounts[$component.component]}else{0}
    if([int]$wave[0].files-ne[int]$component.files){Add-Issue 'WAVE-FILE-COUNT' $component.component "component=$($component.files);wave=$($wave[0].files)"}
    if([int]$wave[0].contracts-ne$expectedContracts){Add-Issue 'WAVE-CONTRACT-COUNT' $component.component "expected=$expectedContracts;wave=$($wave[0].contracts)"}
    if([int]$wave[0].cases-ne$expectedCases){Add-Issue 'WAVE-CASE-COUNT' $component.component "expected=$expectedCases;wave=$($wave[0].cases)"}
}

# Every production file must have at least one audited and traced documentary row.
$coverageByFile=@{}
foreach($row in $coverage){if(-not$coverageByFile.ContainsKey($row.file_id)){$coverageByFile[$row.file_id]=[Collections.Generic.List[object]]::new()};$coverageByFile[$row.file_id].Add($row)}
foreach($file in $production){
    $rows=if($coverageByFile.ContainsKey($file.id)){@($coverageByFile[$file.id])}else{@()}
    if($rows.Count-eq0){Add-Issue 'COVERAGE-MISSING' $file.id $file.path;continue}
    if(@($rows|Where-Object file_state -eq 'AUDITADO').Count-eq0){Add-Issue 'COVERAGE-NOT-AUDITED' $file.id $file.path}
    if(@($rows|Where-Object trace_state -in @('TRAZADO','AUDITADO')).Count-eq0){Add-Issue 'COVERAGE-NOT-TRACED' $file.id $file.path}
    foreach($row in $rows){if($row.path-ne$file.path){Add-Issue 'COVERAGE-PATH-DIFF' $file.id $row.path};Test-Doc $row.document "$($file.id) coverage"}
}
foreach($row in $coverage){if(-not$productionById.ContainsKey($row.file_id)){Add-Issue 'COVERAGE-EXTRA' $row.file_id $row.path}}

# Independently verify focused audit manifests against their physical subsets.
$selfhostExpected=@($production|Where-Object{$_.path-like'selfhost/nsc/*'})
if($selfhostAudit.Count-ne$selfhostExpected.Count){Add-Issue 'SELFHOST-AUDIT-COUNT' 'selfhost-file-audit.csv' "expected=$($selfhostExpected.Count);actual=$($selfhostAudit.Count)"}
$selfhostByFile=@{};foreach($row in $selfhostAudit){if(-not$selfhostByFile.ContainsKey($row.file_id)){$selfhostByFile[$row.file_id]=[Collections.Generic.List[object]]::new()};$selfhostByFile[$row.file_id].Add($row)}
foreach($file in $selfhostExpected){$row=if($selfhostByFile.ContainsKey($file.id)){@($selfhostByFile[$file.id])}else{@()};if($row.Count-ne1){Add-Issue 'SELFHOST-AUDIT-CARDINALITY' $file.id "count=$($row.Count)"}elseif($row[0].sha256-ne$file.sha256){Add-Issue 'SELFHOST-AUDIT-HASH' $file.id $file.path}}
$surfaceRoles=@('BUILD_CONTRACT','SELFHOST_CONTRACT','SHIPPED_DEPENDENCY','SUPPORT_TOOL','VENDORED_DEPENDENCY')
$remainingExpected=@($production|Where-Object role -in $surfaceRoles)
if($remainingAudit.Count-ne$remainingExpected.Count){Add-Issue 'REMAINING-AUDIT-COUNT' 'remaining-surface-file-audit.csv' "expected=$($remainingExpected.Count);actual=$($remainingAudit.Count)"}
$remainingByFile=@{};foreach($row in $remainingAudit){if(-not$remainingByFile.ContainsKey($row.file_id)){$remainingByFile[$row.file_id]=[Collections.Generic.List[object]]::new()};$remainingByFile[$row.file_id].Add($row)}
foreach($file in $remainingExpected){$row=if($remainingByFile.ContainsKey($file.id)){@($remainingByFile[$file.id])}else{@()};if($row.Count-ne1){Add-Issue 'REMAINING-AUDIT-CARDINALITY' $file.id "count=$($row.Count)"}elseif($row[0].sha256-ne$file.sha256){Add-Issue 'REMAINING-AUDIT-HASH' $file.id $file.path}}

# The historical reference remains byte-identical; it is not executed here.
Assert-Unique $historical 'id' 'HISTORICAL-DUPLICATE-ID'
Assert-Unique $historical 'path' 'HISTORICAL-DUPLICATE-PATH'
foreach($row in $historical){
    $absolute=Join-Path $repoRoot $row.path
    if(-not(Test-Path -LiteralPath $absolute)){Add-Issue 'HISTORICAL-MISSING' $row.id $row.path;continue}
    $actual=Lower-Hash $absolute
    if($actual-ne$row.sha256){Add-Issue 'HISTORICAL-HASH' $row.id "$($row.path):expected=$($row.sha256);actual=$actual"}
}

# Verify every hash published by the manifest summary that names an artifact.
$manifest=Get-Content -LiteralPath (Join-Path $inventoryRoot 'manifest-summary.json') -Raw|ConvertFrom-Json
$manifestFiles=[ordered]@{
    production_csv_sha256='production-files.csv';production_json_sha256='production-files.json';components_csv_sha256='components.csv';document_coverage_csv_sha256='document-coverage.csv'
    symbols_csv_sha256='symbols.csv';contracts_csv_sha256='contracts.csv';test_case_design_csv_sha256='test-case-design.csv';historical_tests_csv_sha256='historical-tests-freeze.csv'
    suite_case_allocation_sha256='suite-case-allocation.csv';implementation_waves_sha256='implementation-waves.csv'
}
foreach($entry in $manifestFiles.GetEnumerator()){
    $actual=Lower-Hash (Join-Path $inventoryRoot $entry.Value)
    if([string]$manifest.$($entry.Key)-ne$actual){Add-Issue 'MANIFEST-HASH' $entry.Key "expected=$actual;actual=$($manifest.$($entry.Key))"}
}

$summary=[ordered]@{
    audit='TANDA-13-independent-phase-1-audit'
    timestamp_utc=(Get-Date).ToUniversalTime().ToString('o')
    repository=(Resolve-Path $repoRoot).Path
    branch=(git -C $repoRoot branch --show-current).Trim()
    frozen_revision=($production.revision|Select-Object -Unique)-join';'
    production_reenumerated=$reenumerated.Count
    production_inventory=$production.Count
    components=$components.Count
    symbols=$symbols.Count
    contracts=$contracts.Count
    designed_cases=$cases.Count
    allocated_cases=$allocation.Count
    implementation_waves=(@($waves.wave|Select-Object -Unique)).Count
    historical_files=$historical.Count
    selfhost_files=$selfhostAudit.Count
    remaining_surface_files=$remainingAudit.Count
    protected_files=$protectedHashes.Count
    issue_count=$issues.Count
    result=if($issues.Count-eq0){'PASS'}else{'FAIL'}
    issues=@($issues)
}
$summaryPath=Join-Path $inventoryRoot 'independent-audit-summary.json'
[pscustomobject]$summary|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $summaryPath -Encoding utf8 -NoNewline

if($issues.Count-ne0){
    $issues|Format-Table -AutoSize|Out-String -Width 260|Write-Output
    throw "Independent Phase 1 audit found $($issues.Count) issue(s); see $summaryPath"
}
[pscustomobject]$summary|ConvertTo-Json -Depth 4
