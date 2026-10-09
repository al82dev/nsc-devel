Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Final static gate for the exact Phase 1 objective. This script does not build
# NSC, execute tests, mutate product code, or inspect/deploy anything on C:.
$docsRoot = Split-Path -Parent $PSScriptRoot
$repoRoot = Split-Path -Parent (Split-Path -Parent $docsRoot)
$inventoryRoot = Join-Path $docsRoot 'inventory'
$checks = [Collections.Generic.List[object]]::new()
$issues = [Collections.Generic.List[object]]::new()

function Add-Check([string]$id,[bool]$passed,[string]$expected,[string]$actual,[string]$evidence) {
    $status=if($passed){'PASS'}else{'FAIL'}
    $checks.Add([pscustomobject][ordered]@{id=$id;status=$status;expected=$expected;actual=$actual;evidence=$evidence})
    if(-not$passed){$issues.Add([pscustomobject][ordered]@{check=$id;expected=$expected;actual=$actual})}
}
function Read-Csv([string]$name){return @(Import-Csv -LiteralPath (Join-Path $inventoryRoot $name))}
function Unique-Count($values){return @($values|Select-Object -Unique).Count}
function Empty-Field-Count($rows,[string[]]$fields){
    $count=0
    foreach($row in $rows){foreach($field in $fields){if([string]::IsNullOrWhiteSpace([string]$row.$field)){$count++}}}
    return $count
}
function Lower-Hash([string]$path){return (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()}

$production=Read-Csv 'production-files.csv'
$components=Read-Csv 'components.csv'
$symbols=Read-Csv 'symbols.csv'
$contracts=Read-Csv 'contracts.csv'
$cases=Read-Csv 'test-case-design.csv'
$coverage=Read-Csv 'document-coverage.csv'
$allocation=Read-Csv 'suite-case-allocation.csv'
$waves=Read-Csv 'implementation-waves.csv'
$historical=Read-Csv 'historical-tests-freeze.csv'
$claims=Read-Csv 'claims.csv'
$transversal=Read-Csv 'transversal-contracts.csv'
$stateMachines=Read-Csv 'state-machines.csv'
$stateMap=Read-Csv 'state-machine-contract-map.csv'
$remaining=Read-Csv 'remaining-surface-file-audit.csv'
$parity=@(Import-Csv -LiteralPath (Join-Path $repoRoot 'docs/compiler-optimization/test-infrastructure/test-execution-parity.csv'))
$cursor=Get-Content -LiteralPath (Join-Path $docsRoot 'CURSOR.json') -Raw|ConvertFrom-Json
$independent=Get-Content -LiteralPath (Join-Path $inventoryRoot 'independent-audit-summary.json') -Raw|ConvertFrom-Json
$buildGraph=Get-Content -LiteralPath (Join-Path $inventoryRoot 'build-graph-summary.json') -Raw|ConvertFrom-Json
$includeGraph=Get-Content -LiteralPath (Join-Path $inventoryRoot 'include-graph-summary.json') -Raw|ConvertFrom-Json

Add-Check 'P1-SCOPE-LOCK' (Test-Path (Join-Path $docsRoot 'SCOPE-LOCK.md')) 'present' 'present' 'SCOPE-LOCK.md'
Add-Check 'P1-INDEPENDENT-AUDIT' ($independent.result-eq'PASS'-and[int]$independent.issue_count-eq0) 'PASS;0 issues' "$($independent.result);$($independent.issue_count) issues" 'inventory/independent-audit-summary.json'

$productionOk=$production.Count-eq974-and(Unique-Count $production.id)-eq974-and(Unique-Count $production.path)-eq974-and@($production|Where-Object state -ne 'AUDITADO').Count-eq0
Add-Check 'P1-PRODUCTION-UNIVERSE' $productionOk '974 unique AUDITADO files' "$($production.Count) rows;$((Unique-Count $production.path)) paths;$(@($production|Where-Object state -ne 'AUDITADO').Count) non-audited" 'inventory/production-files.csv'
$coverageIds=@($coverage|Where-Object{$_.file_state-eq'AUDITADO'-and$_.trace_state-in@('TRAZADO','AUDITADO')}|ForEach-Object file_id|Select-Object -Unique)
Add-Check 'P1-DOCUMENT-COVERAGE' ($coverageIds.Count-eq974-and@($production|Where-Object{$_.id-notin$coverageIds}).Count-eq0) '974 audited and traced file IDs' "$($coverageIds.Count) audited/traced IDs" 'inventory/document-coverage.csv'

$allowedExternal=@{
    'ThirdParty.Documentation'='NO APLICA - UPSTREAM';'ThirdParty.zlib'='NO APLICA - UPSTREAM';'ThirdParty.zstd'='NO APLICA - UPSTREAM';'Tooling'='NO APLICA - FUERA DE SUITE NSC'
}
$componentStateErrors=@($components|Where-Object state -ne 'AUDITADO').Count
$reconstructible=@($components|Where-Object reconstructibility -eq 'RECONSTRUIBLE')
$externalErrors=0
foreach($entry in $allowedExternal.GetEnumerator()){
    $row=@($components|Where-Object component -eq $entry.Key)
    if($row.Count-ne1-or$row[0].reconstructibility-ne$entry.Value){$externalErrors++}
}
Add-Check 'P1-COMPONENTS' ($components.Count-eq57-and(Unique-Count $components.component)-eq57-and$componentStateErrors-eq0-and$reconstructible.Count-eq53-and$externalErrors-eq0) '57 audited;53 reconstructible;4 explicit external dispositions' "$($components.Count) total;$($reconstructible.Count) reconstructible;$externalErrors external disposition errors" 'inventory/components.csv;RECONSTRUCTIBILITY.md'

$planMissing=0;$auditMissing=0;$contractClassMissing=0
foreach($wave in $waves){
    $plan=Join-Path $docsRoot $wave.source_test_plan
    if(-not(Test-Path -LiteralPath $plan)){$planMissing++;continue}
    $component=@($components|Where-Object component -eq $wave.component)[0]
    if(-not(Test-Path -LiteralPath (Join-Path $docsRoot $component.audit_report))){$auditMissing++}
    if([int]$wave.cases-gt0){
        $text=Get-Content -LiteralPath $plan -Raw
        $hasValid=$text-match'(?i)válid|valid'
        $hasInvalid=$text-match'(?i)inválid|invalid|malform|error'
        $hasBoundary=$text-match'(?i)borde|boundary|límite|limit'
        if(-not($hasValid-and$hasInvalid-and$hasBoundary)){$contractClassMissing++}
    } elseif(-not$allowedExternal.ContainsKey($wave.component)){$contractClassMissing++}
}
Add-Check 'P1-COMPONENT-TEST-PLANS' ($waves.Count-eq57-and$planMissing-eq0-and$auditMissing-eq0-and$contractClassMissing-eq0) '57 plans and audits; valid/invalid/boundary for 53 owned components;4 explicit N/A' "$($waves.Count) waves;$planMissing missing plans;$auditMissing missing audits;$contractClassMissing class/disposition errors" 'inventory/implementation-waves.csv;test-plans/**;audits/**'

$symbolFields=@('symbol_id','file_id','path','component','kind','qualified_name','line','responsibility','inputs','preconditions','outputs','invariants','errors','side_effects','dependencies','observability','test_layer','state','document','section')
$symbolEmpty=Empty-Field-Count $symbols $symbolFields
$symbolDuplicates=$symbols.Count-(Unique-Count $symbols.symbol_id)
$remainingDisposition=@{};foreach($row in $remaining){$remainingDisposition[$row.file_id]=$row.disposition}
$unjustifiedSymbols=0
foreach($symbol in $symbols|Where-Object{[string]::IsNullOrWhiteSpace($_.contract_id)}){
    $fileIds=@($symbol.file_id-split';'|Where-Object{$_})
    foreach($fileId in $fileIds){if(-not$remainingDisposition.ContainsKey($fileId)-or$remainingDisposition[$fileId]-notin@('UPSTREAM','EXCLUIDA')){$unjustifiedSymbols++;break}}
}
Add-Check 'P1-SYMBOL-INVENTORY' ($symbols.Count-eq8418-and$symbolDuplicates-eq0-and$symbolEmpty-eq0-and$unjustifiedSymbols-eq0) '8418 unique complete symbols;all uncontracted symbols explicitly upstream/excluded' "$($symbols.Count) rows;$symbolDuplicates duplicates;$symbolEmpty empty required fields;$unjustifiedSymbols unjustified" 'inventory/symbols.csv;inventory/remaining-surface-file-audit.csv'

$contractFields=@('contract_id','component','name','source_symbols','inputs','preconditions','outputs','postconditions','invariants','errors','side_effects','dependencies','observable_oracle','test_layer','status','document','section')
$contractEmpty=Empty-Field-Count $contracts $contractFields
$contractDuplicates=$contracts.Count-(Unique-Count $contracts.contract_id)
$contractsWithoutCases=@($contracts|Where-Object{$_.contract_id-notin$cases.contract_id}).Count
Add-Check 'P1-CONTRACT-INVENTORY' ($contracts.Count-eq1309-and$contractDuplicates-eq0-and$contractEmpty-eq0-and$contractsWithoutCases-eq0) '1309 unique complete contracts;every contract has designed cases' "$($contracts.Count) rows;$contractDuplicates duplicates;$contractEmpty empty required fields;$contractsWithoutCases without cases" 'inventory/contracts.csv;inventory/test-case-design.csv'

$caseFields=@('case_id','contract_id','component','objective','input_class','setup','input','expected','dependencies','layer','frequency','status','document','section')
$caseEmpty=Empty-Field-Count $cases $caseFields
$caseDuplicates=$cases.Count-(Unique-Count $cases.case_id)
Add-Check 'P1-CASE-DESIGN' ($cases.Count-eq10193-and$caseDuplicates-eq0-and$caseEmpty-eq0) '10193 unique fully specified logical cases' "$($cases.Count) rows;$caseDuplicates duplicates;$caseEmpty empty required fields" 'inventory/test-case-design.csv'
$allocationDiff=@($cases|Where-Object{$_.case_id-notin$allocation.case_id}).Count+@($allocation|Where-Object{$_.case_id-notin$cases.case_id}).Count
Add-Check 'P1-SUITE-ALLOCATION' ($allocation.Count-eq10193-and(Unique-Count $allocation.case_id)-eq10193-and$allocationDiff-eq0) '10193 cases allocated exactly once' "$($allocation.Count) rows;$((Unique-Count $allocation.case_id)) unique;$allocationDiff set differences" 'inventory/suite-case-allocation.csv'

$transversalMissing=@($contracts|Where-Object{$_.contract_id-notin$transversal.contract_id}).Count
$badStateRefs=@($stateMap|Where-Object{$_.contract_id-notin$contracts.contract_id-or$_.state_machine_id-notin$stateMachines.state_machine_id}).Count
Add-Check 'P1-STATES-AND-TRANSVERSALS' ($transversal.Count-eq1309-and(Unique-Count $transversal.contract_id)-eq1309-and$transversalMissing-eq0-and$stateMachines.Count-eq12-and$badStateRefs-eq0) '1309 transversal classifications;12 state machines;all mappings resolvable' "$($transversal.Count) transversal;$transversalMissing missing;$($stateMachines.Count) machines;$badStateRefs bad mappings" 'inventory/transversal-contracts.csv;inventory/state-machines.csv;inventory/state-machine-contract-map.csv'

$hostComponents=@($components.component|Where-Object{$_-like'Host.*'}|Select-Object -Unique)
$orderRows=Read-Csv 'host-component-order.csv'
$orderedComponents=@($orderRows.components|ForEach-Object{$_-split';'}|Where-Object{$_}|Select-Object -Unique)
$graphOk=[int]$buildGraph.missing-eq0-and[int]$buildGraph.extra-eq0-and[int]$buildGraph.host_sources-eq[int]$buildGraph.mapped_host_sources-and[int]$includeGraph.unclassified-eq0-and@($hostComponents|Where-Object{$_-notin$orderedComponents}).Count-eq0
Add-Check 'P1-DEPENDENCY-GRAPHS' $graphOk 'zero build/include gaps and all Host components ordered' "build missing=$($buildGraph.missing);extra=$($buildGraph.extra);mapped=$($buildGraph.mapped_host_sources)/$($buildGraph.host_sources);include unclassified=$($includeGraph.unclassified);ordered=$($orderedComponents.Count)/$($hostComponents.Count)" 'inventory/build-graph-summary.json;inventory/include-graph-summary.json;inventory/host-component-order.csv'

$claimFields=@('claim_id','classification','statement','repository','path','revision','evidence_type','document','section','status')
$claimEmpty=Empty-Field-Count $claims $claimFields
Add-Check 'P1-CLAIMS-PROVENANCE' ($claims.Count-eq273-and(Unique-Count $claims.claim_id)-eq273-and$claimEmpty-eq0-and@($claims|Where-Object status -ne 'AUDITADO').Count-eq0) '273 unique audited claims with provenance' "$($claims.Count) rows;$claimEmpty empty required fields" 'inventory/claims.csv'

$contradictionText=Get-Content -LiteralPath (Join-Path $docsRoot 'CONTRADICTIONS.md') -Raw
$contradictionIds=@([regex]::Matches($contradictionText,'CON-\d{4}')|ForEach-Object Value|Select-Object -Unique)
$openStates=@([regex]::Matches($contradictionText,'ABIERTA')|ForEach-Object Value).Count
Add-Check 'P1-CONTRADICTIONS' ($contradictionIds.Count-eq17-and$openStates-eq17) '17 registered contradictions with explicit ABIERTA state' "$($contradictionIds.Count) IDs;$openStates ABIERTA states" 'CONTRADICTIONS.md'
$blockerText=Get-Content -LiteralPath (Join-Path $docsRoot 'BLOCKERS.md') -Raw
$blockerIds=@([regex]::Matches($blockerText,'BLK-\d{4}')|ForEach-Object Value|Select-Object -Unique)
Add-Check 'P1-BLOCKERS' ($blockerIds.Count-eq0-and$blockerText-match'No hay bloqueos documentales abiertos') '0 open documentary blockers' "$($blockerIds.Count) blocker IDs" 'BLOCKERS.md'

$parityOk=$parity.Count-eq684-and(Unique-Count $parity.ctest_id)-eq684
Add-Check 'P1-HISTORICAL-TEST-SET' $parityOk '684 unique frozen CTest IDs' "$($parity.Count) rows;$((Unique-Count $parity.ctest_id)) unique" 'protected test-execution-parity.csv'
$historicalHashErrors=0
foreach($row in $historical){$path=Join-Path $repoRoot $row.path;if(-not(Test-Path -LiteralPath $path)-or(Lower-Hash $path)-ne$row.sha256){$historicalHashErrors++}}
Add-Check 'P1-HISTORICAL-FILES' ($historical.Count-eq1658-and(Unique-Count $historical.path)-eq1658-and$historicalHashErrors-eq0) '1658 unique frozen files byte-identical' "$($historical.Count) rows;$historicalHashErrors hash/missing errors" 'inventory/historical-tests-freeze.csv'

$protectedHashes=[ordered]@{
    'docs/compiler-optimization/test-infrastructure/test-execution-parity.csv'='96c6e1564f973b36f4ab8cdd2b5df515e5128ee1345dd622024d15e9799f606d'
    'docs/compiler-optimization/test-infrastructure/test-execution-parity.json'='2dac516432a277fd4c0c2cd3a4868d348a6f626732f01243047919e32faa727a'
    'docs/compiler-optimization/test-infrastructure/test-execution-parity.md'='27802a8867f752fb6efd5b063264626d837e43cbea0a5362f5aa166ac4feaa4a'
    'tests/harness/nsc_test/suites.py'='407b534d216270b052aa35416d01d690219feba2b8c6d6438fec3634c5418b38'
    'tests/harness/run_nsc_tests.py'='e5ecf33859092b134b83f2665c659bc5691def40e3e31def39de9a4cf9946151'
    'tests/harness/tests/test_suite_adapter.py'='fef8fe5d953b0d2d3dbbf747d97719b8fc12f829d2cef0334a5025389db68dc0'
}
$protectedErrors=0;foreach($entry in $protectedHashes.GetEnumerator()){if((Lower-Hash (Join-Path $repoRoot $entry.Key))-ne$entry.Value){$protectedErrors++}}
Add-Check 'P1-PROTECTED-PREEXISTING' ($protectedErrors-eq0) '6 pre-existing files byte-identical' "$protectedErrors mismatches" 'SCOPE-LOCK.md;SHA-256'

$phaseStart=(git -C $repoRoot rev-parse 'b0bf949be^').Trim()
$phaseCommits=@(git -C $repoRoot rev-list --reverse "$phaseStart..HEAD")
$outsideCommitPaths=[Collections.Generic.List[string]]::new()
foreach($commit in $phaseCommits){foreach($path in @(git -C $repoRoot diff-tree --no-commit-id --name-only -r $commit)){if($path-notlike'docs/compiler-testing-reengineering/*'){$outsideCommitPaths.Add("$commit`:$path")}}}
Add-Check 'P1-COMMIT-SCOPE' ($phaseCommits.Count-gt0-and$outsideCommitPaths.Count-eq0) 'all Phase 1 commits restricted to the authorized documentation root' "$($outsideCommitPaths.Count) outside paths" 'git history b0bf949be..HEAD'

$status=@(git -C $repoRoot status --porcelain=v1)
$outsideWorktree=@()
foreach($line in $status){$path=$line.Substring(3).Replace('\','/');if($path-notlike'docs/compiler-testing-reengineering/*'){$outsideWorktree+=$path}}
$protectedPaths=@($protectedHashes.Keys|ForEach-Object{$_.Replace('\','/')})
$unexpectedOutside=@($outsideWorktree|Where-Object{$_-notin$protectedPaths})
Add-Check 'P1-WORKTREE-SCOPE' ($unexpectedOutside.Count-eq0-and(Unique-Count $outsideWorktree)-eq6) 'only the 6 protected pre-existing paths outside Phase 1 docs' "$((Unique-Count $outsideWorktree)) outside paths;$($unexpectedOutside.Count) unexpected" 'git status;SCOPE-LOCK.md'

$noImplementation=[int]$cursor.test_case_design.implemented-eq0-and[int]$cursor.test_case_design.executed-eq0-and[int]$cursor.tests.new_tests_created-eq0-and$outsideCommitPaths.Count-eq0
Add-Check 'P1-NO-IMPLEMENTATION-OR-EXECUTION' $noImplementation 'implemented=0;executed=0;new_tests=0;document-only commits' "implemented=$($cursor.test_case_design.implemented);executed=$($cursor.test_case_design.executed);new_tests=$($cursor.tests.new_tests_created);outside_paths=$($outsideCommitPaths.Count)" 'CURSOR.json;git history;SCOPE-LOCK.md'

$requiredArtifacts=@('SCOPE-LOCK.md','PHASE-1-MASTER-PLAN.md','TRACKING.md','CURSOR.json','INDEX.md','RECONSTRUCTIBILITY.md','CONTRADICTIONS.md','BLOCKERS.md','architecture/COMPONENT-GRAPH.md','architecture/NEW-SUITE-ARCHITECTURE.md','architecture/EXECUTION-PROFILES.md','architecture/IMPLEMENTATION-WAVES.md','audits/TANDA-13-INDEPENDENT-PHASE-1-AUDIT.md','audits/PHASE-1-COMPLETION-AUDIT.md')
$missingArtifacts=@($requiredArtifacts|Where-Object{-not(Test-Path -LiteralPath (Join-Path $docsRoot $_))})
Add-Check 'P1-REQUIRED-ARTIFACTS' ($missingArtifacts.Count-eq0) 'all normative, architecture, state and audit artifacts present' "$($missingArtifacts.Count) missing" ($requiredArtifacts-join';')

$summary=[ordered]@{
    gate='NativeSharp-NSC-Phase-1-completion'
    repository=(Resolve-Path $repoRoot).Path
    branch=(git -C $repoRoot branch --show-current).Trim()
    frozen_product_revision=($production.revision|Select-Object -Unique)-join';'
    checks_total=$checks.Count
    checks_passed=@($checks|Where-Object status -eq 'PASS').Count
    checks_failed=@($checks|Where-Object status -eq 'FAIL').Count
    result=if($issues.Count-eq0){'READY_FOR_USER_REVIEW'}else{'NOT_READY'}
    user_closure_required=$true
    static_only=$true
    build_executed=$false
    tests_executed=$false
    checks=@($checks)
    issues=@($issues)
}
$summaryPath=Join-Path $inventoryRoot 'phase-1-completion-gate.json'
[pscustomobject]$summary|ConvertTo-Json -Depth 7|Set-Content -LiteralPath $summaryPath -Encoding utf8 -NoNewline
if($issues.Count-ne0){$issues|Format-Table -AutoSize|Out-String -Width 260|Write-Output;throw "Phase 1 completion gate failed: $($issues.Count) check(s)"}
[pscustomobject]$summary|ConvertTo-Json -Depth 4
