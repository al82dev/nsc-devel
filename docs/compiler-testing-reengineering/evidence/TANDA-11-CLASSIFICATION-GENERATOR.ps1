Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = Join-Path (Split-Path -Parent $PSScriptRoot) 'inventory'
$contractsPath = Join-Path $root 'contracts.csv'
$contracts = @(Import-Csv -LiteralPath $contractsPath | Sort-Object component, contract_id)

function Test-Text {
    param([string]$Text, [string]$Pattern)
    return $Text -match $Pattern
}

function Join-Labels {
    param([System.Collections.Generic.List[string]]$Labels, [string]$Empty = 'NOT_APPLICABLE')
    if ($Labels.Count -eq 0) { return $Empty }
    return (($Labels | Select-Object -Unique) -join ';')
}

function Add-IfMatch {
    param(
        [System.Collections.Generic.List[string]]$Labels,
        [string]$Text,
        [string]$Pattern,
        [string]$Label
    )
    if (Test-Text $Text $Pattern) { $Labels.Add($Label) }
}

$transversal = [System.Collections.Generic.List[object]]::new()
$errors = [System.Collections.Generic.List[object]]::new()
$stateMap = [System.Collections.Generic.List[object]]::new()
$mappingOrdinal = 0
$stateMappingOrdinal = 0

foreach ($contract in $contracts) {
    $mappingOrdinal++
    $all = @(
        $contract.name,
        $contract.inputs,
        $contract.preconditions,
        $contract.outputs,
        $contract.postconditions,
        $contract.invariants,
        $contract.errors,
        $contract.side_effects,
        $contract.dependencies,
        $contract.observable_oracle
    ) -join ' '

    $diagnostics = [System.Collections.Generic.List[string]]::new()
    $hasDeclaredError = $contract.errors -and $contract.errors -notmatch '^(None|None directly)$'
    if (-not $hasDeclaredError) {
        $diagnostics.Add('NO_DECLARED_ERROR')
    } else {
        $diagnostics.Add('DECLARED_ERROR_CHANNEL')
        Add-IfMatch $diagnostics $all '(?i)diagnostic|report|ExpectedToken|ExpectedExpression|InvalidOperand|InvalidType|DuplicateSymbol' 'DIAGNOSTIC_EMISSION'
        Add-IfMatch $diagnostics $all '(?i)first[- ]error|first error|latch' 'FIRST_ERROR_LATCH'
        Add-IfMatch $diagnostics $all '(?i)recover|progress|continue after|advances diagnostics' 'RECOVER_AND_CONTINUE'
        Add-IfMatch $diagnostics $all '(?i)rollback|no partial|no stale|scope.*restore|restoration|transaction' 'ROLLBACK_OR_NO_PUBLICATION'
        Add-IfMatch $diagnostics $all '(?i)fallback|reuse.*last|preserve.*valid' 'FALLBACK_OR_LAST_VALID'
        Add-IfMatch $diagnostics $all '(?i)nullopt|optional|sentinel|yields empty|returns empty|false result' 'SENTINEL_OR_OPTIONAL'
        Add-IfMatch $diagnostics $contract.errors '(?i)propagat|may throw|allocation failure' 'PROPAGATION'
        if ($diagnostics.Count -eq 1) { $diagnostics.Add('STOP_AT_BOUNDARY') }
    }

    $identity = [System.Collections.Generic.List[string]]::new()
    Add-IfMatch $identity $all '(?i)identity|\bid\b|ordinal|provenance|symbol key|path key' 'IDENTITY'
    Add-IfMatch $identity $all '(?i)determin|stable|canonical|repeat|byte-identical|idempot' 'DETERMINISM'
    Add-IfMatch $identity $all '(?i)\bcache\b|intern|memoiz' 'CACHE'
    Add-IfMatch $identity $all '(?i)fingerprint|hash|SHA-256|digest' 'FINGERPRINT_OR_HASH'
    Add-IfMatch $identity $all '(?i)unique|dedup|duplicate|collision' 'UNIQUENESS_OR_DEDUPLICATION'

    $ownership = [System.Collections.Generic.List[string]]::new()
    Add-IfMatch $ownership $all '(?i)ownership|\bowned\b|owner' 'OWNERSHIP'
    Add-IfMatch $ownership $all '(?i)borrow|lifetime|remain alive|stays alive|outlive' 'BORROW_OR_LIFETIME'
    Add-IfMatch $ownership $all '(?i)transfer|\bmove\b|\bcopy\b|clone' 'TRANSFER_MOVE_OR_COPY'
    Add-IfMatch $ownership $all '(?i)handle|resource|release|close|destroy|destruct|cleanup|leak|RAII' 'RESOURCE_LIFECYCLE'
    Add-IfMatch $ownership $all '(?i)arena|allocat|deallocat|heap|stack allocation' 'ALLOCATION_OR_ARENA'
    Add-IfMatch $ownership $all '(?i)scope|restore|unwind|bracket' 'SCOPE_OR_CLEANUP'

    $abi = [System.Collections.Generic.List[string]]::new()
    Add-IfMatch $abi $all '(?i)\bABI\b|calling convention|data layout|layout|alignment|offset|bit[- ]field|Win64 typing' 'ABI_OR_LAYOUT'
    Add-IfMatch $abi $all '(?i)metadata|NSMETA|schema|sidecar|catalogue|catalog' 'METADATA_OR_SCHEMA'
    Add-IfMatch $abi $all '(?i)artifact|publish|publication|manifest|header|library|archive' 'ARTIFACT_OR_PUBLICATION'
    Add-IfMatch $abi $all '(?i)\bCOFF\b|\bPE\b|object file|linker|link contract|import library|export' 'OBJECT_OR_LINK_FORMAT'
    Add-IfMatch $abi $all '(?i)runtime|entry point|startup|support module' 'RUNTIME_CONTRACT'
    Add-IfMatch $abi $all '(?i)\bPDB\b|debug info|debug value|CodeView|DWARF' 'DEBUG_SYMBOL_CONTRACT'

    $concurrency = [System.Collections.Generic.List[string]]::new()
    Add-IfMatch $concurrency $all '(?i)cancel|cancelled|cancellation' 'CANCELLATION'
    Add-IfMatch $concurrency $all '(?i)concurr|thread|worker|parallel|async|race|pool' 'CONCURRENCY'
    Add-IfMatch $concurrency $all '(?i)atomic|mutex|monitor|semaphore|event lifecycle|\block\b|synchroniz|TLS' 'ATOMIC_OR_SYNCHRONIZATION'
    Add-IfMatch $concurrency $all '(?i)order|sequence|wave|topolog|FIFO|LIFO|deterministic merge' 'ORDERING'
    Add-IfMatch $concurrency $all '(?i)schedul|job|queue|submission|dispatch' 'SCHEDULING'
    Add-IfMatch $concurrency $all '(?i)wait|join|detach|deadline|timeout|timed-out|sleep' 'WAIT_OR_DEADLINE'

    $configuration = [System.Collections.Generic.List[string]]::new()
    Add-IfMatch $configuration $all '(?i)option|flag|argument|command line|CLI' 'OPTIONS_OR_FLAGS'
    Add-IfMatch $configuration $all '(?i)\bmode\b|\bprofile\b|subsystem|strict mode|worker mode' 'MODE_OR_PROFILE'
    Add-IfMatch $configuration $all '(?i)optimiz|\bO[0-3]\b|pass pipeline|ThinLTO' 'OPTIMIZATION_CONFIGURATION'
    Add-IfMatch $configuration $all '(?i)target|platform|architecture|CPU|Win32|Windows|x64|ARM64|triple' 'TARGET_OR_PLATFORM'
    Add-IfMatch $configuration $all '(?i)language|edition|feature|capability|compatibility' 'LANGUAGE_OR_FEATURE_POLICY'
    Add-IfMatch $configuration $all '(?i)environment|toolchain|SDK|compiler path|linker selection|runtime path' 'ENVIRONMENT_OR_TOOLCHAIN'
    Add-IfMatch $configuration $all '(?i)matrix|range|minimum|maximum|limit|budget|version' 'MATRIX_RANGE_OR_LIMIT'

    # Machine membership is deliberately narrower than transversal classification.
    # Generic component templates in preconditions/postconditions must not turn every
    # contract in a component into the same state machine.  Names and owned symbols
    # are the stable evidence used here.
    $stateText = @($contract.name, $contract.source_symbols) -join ' '
    $stateModels = [System.Collections.Generic.List[string]]::new()
    if ($contract.component -in @('Host.Lexer', 'Host.Parser') -or (Test-Text $stateText '(?i)cursor|syntax recovery|recovery progress|token stream')) {
        $stateModels.Add('SM-01-CURSOR-RECOVERY')
    }
    if ((Test-Text $stateText '(?i)first[- ]error|first error|error lifecycle|diagnostics publication|syntax recovery|recovery progress') -or
        ($contract.component -eq 'Host.Diagnostics' -and (Test-Text $stateText '(?i)diagnostic identity|diagnostic record|report storage|error budget|engine queries|suppression summary'))) {
        $stateModels.Add('SM-02-DIAGNOSTIC-ACCUMULATOR')
    }
    if (Test-Text $stateText '(?i)transaction|staging|stage seeding|rollback|commit|publication|publish|writer|write and verification|creation|emission|temporary cleanup') {
        $stateModels.Add('SM-03-TRANSACTIONAL-PUBLICATION')
    }
    if (Test-Text $stateText '(?i)\bcache\b|intern|memoiz') {
        $stateModels.Add('SM-04-CACHE-LIFECYCLE')
    }
    if ($contract.component -notin @('Host.Diagnostics', 'Host.Interop', 'Host.Language', 'Host.Lexer', 'Host.Parser', 'Host.Sema', 'Host.Target') -and
        (Test-Text $stateText '(?i)ownership|\bowned\b|borrow|lifetime|arena|handle|allocation|resource|cleanup|destruct|RAII|exceptional release|heap allocation and release')) {
        $stateModels.Add('SM-05-OWNED-RESOURCE-LIFECYCLE')
    }
    if ($contract.component -in @('Host.CFG', 'Host.Dominance', 'Host.SSA') -or (Test-Text $stateText '(?i)graph|tree|\bCFG\b|dominance|\bSSA\b|topology|edge|topolog|dependency closure|reachability|predecessor|successor')) {
        $stateModels.Add('SM-06-GRAPH-EVOLUTION')
    }
    if (Test-Text $stateText '(?i)pipeline|lowering|compilation stages|frontend compilation|project compilation orchestration|compilation graph|pass manager|optimization|generation|materialization|codegen|emission') {
        $stateModels.Add('SM-07-PIPELINE-TRANSFORMATION')
    }
    if (Test-Text $stateText '(?i)job|worker|thread|cancel|concurr|parallel|atomic memory|atomic 32|atomic operations|interlocked|mutex|monitor|semaphore|event lifecycle|submission|worker-pool') {
        $stateModels.Add('SM-08-ASYNC-JOB-LIFECYCLE')
    }
    if ($contract.component -in @('Host.Basic', 'Host.Driver', 'Host.Linker', 'Host.LLVM', 'Host.nsc', 'Host.Runtime') -and
        (Test-Text $stateText '(?i)\bprocess\b|child process|process launch|process execution|process exit|process identity|entry point|crash boundary|SEH boundary|supervisor')) {
        $stateModels.Add('SM-09-PROCESS-LIFECYCLE')
    }
    if (Test-Text $stateText '(?i)snapshot|document update|semantic index|version monotonic|last model|overlay') {
        $stateModels.Add('SM-10-SNAPSHOT-VERSION-LIFECYCLE')
    }
    if (Test-Text $stateText '(?i)\boptions?\b|\bprofile\b|target selection|configuration|SDK mapping|build settings|environment|required-file capability|command dispatch|\bmode\b|feature selection|policy schema|compatibility policy|deprecation.*policy|inlining policy|floating point lowering policy|error-log policy') {
        $stateModels.Add('SM-11-CONFIGURATION-RESOLUTION')
    }
    if (Test-Text $stateText '(?i)builder|verifier|reset and first-error|build input') {
        $stateModels.Add('SM-12-BUILDER-VERIFIER-LIFECYCLE')
    }
    if ($stateModels.Count -eq 0) { $stateModels.Add('STATELESS_OR_VALUE_CONTRACT') }

    $errorEvidence = @($contract.name, $contract.errors) -join ' '
    $errorClasses = [System.Collections.Generic.List[string]]::new()
    if (-not $hasDeclaredError) {
        $errorClasses.Add('NO_DECLARED_ERROR')
    } else {
        if ($contract.component -in @('Host.Lexer', 'Host.Parser') -or (Test-Text $errorEvidence '(?i)syntax|token|parse|grammar')) { $errorClasses.Add('SYNTAX_OR_PARSE') }
        if ($contract.component -in @('Host.Sema', 'Host.Frontend') -or (Test-Text $errorEvidence '(?i)semantic|type error|ownership|borrow|conversion|overload')) { $errorClasses.Add('SEMANTIC_OR_TYPE') }
        if (Test-Text $contract.errors '(?i)malformed|invalid|unsupported|duplicate|conflict|out-of-range|boundary|truncated|schema|verification|coherent|cycle') { $errorClasses.Add('VALIDATION_OR_SCHEMA') }
        if (Test-Text $errorEvidence '(?i)filesystem|file read|file write|path traversal|path confinement|ostream|stream failure|directory|artifact') { $errorClasses.Add('IO_FILESYSTEM_OR_ARTIFACT') }
        if (Test-Text $errorEvidence '(?i)allocation|memory|resource|handle|leak') { $errorClasses.Add('RESOURCE_OR_ALLOCATION') }
        if (Test-Text $errorEvidence '(?i)LLVM verification|toolchain|linker|compiler dispatch|backend|object generation') { $errorClasses.Add('TOOLCHAIN_OR_BACKEND') }
        if (Test-Text $errorEvidence '(?i)process|Win32|platform-fault|SEH|child process|operating-system') { $errorClasses.Add('PROCESS_OR_PLATFORM') }
        if (Test-Text $contract.errors '(?i)cancel|timed-out|timeout|deadline') { $errorClasses.Add('CANCELLATION_OR_TIMEOUT') }
        if (Test-Text $errorEvidence '(?i)budget|limit|depth|capacity|size guard|overflow|out-of-range') { $errorClasses.Add('BUDGET_OR_LIMIT') }
        if (Test-Text $errorEvidence '(?i)verifier|verification|invariant|internal|structural|first error') { $errorClasses.Add('INVARIANT_OR_VERIFICATION') }
        if ($errorClasses.Count -eq 0) { $errorClasses.Add('DECLARED_DOMAIN_FAILURE') }
    }

    $recovery = [System.Collections.Generic.List[string]]::new()
    if (-not $hasDeclaredError) {
        $recovery.Add('NOT_APPLICABLE')
    } else {
        Add-IfMatch $recovery $all '(?i)recover|progress|continue after|advances diagnostics' 'RECOVER_AND_CONTINUE'
        Add-IfMatch $recovery $all '(?i)rollback|no partial|no stale|scope.*restore|restoration|transaction' 'ROLLBACK_OR_NO_PUBLICATION'
        Add-IfMatch $recovery $all '(?i)fallback|reuse.*last|preserve.*valid' 'FALLBACK_OR_LAST_VALID'
        Add-IfMatch $recovery $all '(?i)nullopt|optional|sentinel|yields empty|returns empty|false result' 'SENTINEL_OR_OPTIONAL'
        Add-IfMatch $recovery $all '(?i)first[- ]error|first error|latch' 'FIRST_ERROR_LATCH'
        Add-IfMatch $recovery $contract.errors '(?i)propagat|may throw|allocation failure' 'PROPAGATE'
        if ($recovery.Count -eq 0) { $recovery.Add('STOP_AT_BOUNDARY') }
    }

    $stateLabelText = Join-Labels $stateModels 'STATELESS_OR_VALUE_CONTRACT'
    $transversal.Add([pscustomobject][ordered]@{
        mapping_id = ('TX-{0:D4}' -f $mappingOrdinal)
        contract_id = $contract.contract_id
        component = $contract.component
        diagnostics_recovery = Join-Labels $diagnostics
        identity_determinism_cache = Join-Labels $identity
        ownership_lifetime_transfer = Join-Labels $ownership
        abi_metadata_artifacts = Join-Labels $abi
        cancellation_concurrency_ordering = Join-Labels $concurrency
        options_modes_matrix = Join-Labels $configuration
        state_models = $stateLabelText
        source_document = $contract.document
        source_section = $contract.section
        status = 'AUDITADO'
    })

    $errors.Add([pscustomobject][ordered]@{
        contract_id = $contract.contract_id
        component = $contract.component
        declared_error = $contract.errors
        error_classes = Join-Labels $errorClasses
        recovery_policies = Join-Labels $recovery
        error_oracle = $contract.observable_oracle
        source_document = $contract.document
        source_section = $contract.section
        status = 'AUDITADO'
    })

    foreach ($stateModel in ($stateModels | Where-Object { $_ -ne 'STATELESS_OR_VALUE_CONTRACT' } | Select-Object -Unique)) {
        $stateMappingOrdinal++
        $stateMap.Add([pscustomobject][ordered]@{
            mapping_id = ('SMAP-{0:D4}' -f $stateMappingOrdinal)
            state_machine_id = $stateModel
            contract_id = $contract.contract_id
            component = $contract.component
            transition_evidence = ('{0} | post: {1} | effects: {2}' -f $contract.name, $contract.postconditions, $contract.side_effects)
            source_document = $contract.document
            source_section = $contract.section
            status = 'AUDITADO'
        })
    }
}

$stateMachines = @(
    [pscustomobject][ordered]@{state_machine_id='SM-01-CURSOR-RECOVERY';name='Cursor recognition and recovery';states='positioned;recognizing;accepted;diagnosed;recovering;EOF';transitions='consume/advance;accept;diagnose;synchronize;terminate';invariants='Every path advances or terminates; ranges refer to consumed source';failure_policy='Emit the owned diagnostic and recover only at a declared boundary';oracle='Exact cursor/range/diagnostic/progress sequence';future_layer='component';status='AUDITADO'},
    [pscustomobject][ordered]@{state_machine_id='SM-02-DIAGNOSTIC-ACCUMULATOR';name='Diagnostic accumulation and first-error latch';states='empty;collecting;suppressed;failed;reset';transitions='report;merge;suppress;latch;reset';invariants='Counts and first error are stable; reset returns to empty';failure_policy='Preserve the first owned error and explicit suppression state';oracle='Exact records counts first error suppression and reset';future_layer='unit';status='AUDITADO'},
    [pscustomobject][ordered]@{state_machine_id='SM-03-TRANSACTIONAL-PUBLICATION';name='Transactional artifact publication';states='absent;staged;preflighted;committing;published;rolling_back;failed';transitions='stage;validate;commit;rollback;cleanup';invariants='Consumers never observe a partial accepted set; confinement holds';failure_policy='Rollback or leave the prior published state intact';oracle='Filesystem and manifest snapshot before during and after failure';future_layer='integration';status='AUDITADO'},
    [pscustomobject][ordered]@{state_machine_id='SM-04-CACHE-LIFECYCLE';name='Cache identity and invalidation';states='empty;miss;building;ready;hit;invalidated;failed';transitions='lookup;construct;publish;reuse;invalidate;discard';invariants='Key identity and producer revision select the value; failed builds are not ready hits';failure_policy='Do not publish failed or stale values';oracle='Hit/miss/build counters identity and returned object equivalence';future_layer='component';status='AUDITADO'},
    [pscustomobject][ordered]@{state_machine_id='SM-05-OWNED-RESOURCE-LIFECYCLE';name='Owned resource and lifetime transfer';states='uninitialized;owned;borrowed;transferred;released;destroyed;failed';transitions='allocate;borrow;move/copy;release;cleanup';invariants='Single owner where required; borrow never outlives owner; cleanup is balanced';failure_policy='Unwind or clean every acquired resource without double release';oracle='Ownership identities lifetime boundaries and release counts';future_layer='component';status='AUDITADO'},
    [pscustomobject][ordered]@{state_machine_id='SM-06-GRAPH-EVOLUTION';name='Graph construction transformation and verification';states='empty;discovering;linked;ordered;transformed;verified;rejected';transitions='insert;link;traverse;order;rewrite;verify';invariants='Edges endpoints and indexes agree; ordering and reachability are deterministic';failure_policy='Latch structural failure and do not expose an accepted malformed graph';oracle='Independent node/edge/order/reachability model';future_layer='component';status='AUDITADO'},
    [pscustomobject][ordered]@{state_machine_id='SM-07-PIPELINE-TRANSFORMATION';name='Compiler pipeline transformation';states='input;admitted;lowered;optimized;verified;emitted;failed';transitions='admit;lower;transform;verify;emit';invariants='Each consumer observes only a valid producer output; provenance survives transitions';failure_policy='Stop at the named stage and publish no downstream success';oracle='Stage result diagnostics provenance and produced artifact set';future_layer='integration';status='AUDITADO'},
    [pscustomobject][ordered]@{state_machine_id='SM-08-ASYNC-JOB-LIFECYCLE';name='Concurrent job scheduling and cancellation';states='created;queued;running;cancelling;completed;failed;discarded;joined';transitions='submit;start;cancel;complete;arbitrate;wait;reuse';invariants='Dependencies precede consumers; completion is indexed; cancellation is monotone';failure_policy='Arbitrate deterministically and discard work that cannot be published';oracle='Start/completion/cancellation order and visible results';future_layer='integration';status='AUDITADO'},
    [pscustomobject][ordered]@{state_machine_id='SM-09-PROCESS-LIFECYCLE';name='Process launch supervision and exit';states='prepared;launched;running;exited;faulted;timed_out;collected';transitions='quote/prepare;launch;wait;terminate;collect;map exit';invariants='Arguments and streams remain attributable; exit/fault identity is preserved';failure_policy='Map platform failure once and preserve the producer exit result';oracle='Arguments streams timing fault and exact exit code';future_layer='process';status='AUDITADO'},
    [pscustomobject][ordered]@{state_machine_id='SM-10-SNAPSHOT-VERSION-LIFECYCLE';name='Snapshot and semantic-index publication';states='absent;opened;updated;analyzing;valid;invalid;closed';transitions='open;replace;analyze;publish;reuse last valid;close';invariants='Versions increase monotonically; a snapshot is immutable after publication';failure_policy='Expose current diagnostics while retaining only an explicitly permitted last valid model';oracle='Version document bytes index identity and diagnostic sequence';future_layer='component';status='AUDITADO'},
    [pscustomobject][ordered]@{state_machine_id='SM-11-CONFIGURATION-RESOLUTION';name='Configuration option and target resolution';states='defaults;parsed;merged;validated;resolved;rejected';transitions='read defaults;parse;override;validate;select';invariants='Precedence is explicit; unknown conflicting or out-of-range input never silently wins';failure_policy='Reject at the owning boundary without hidden fallback unless specified';oracle='Resolved configuration provenance and exact diagnostic';future_layer='component';status='AUDITADO'},
    [pscustomobject][ordered]@{state_machine_id='SM-12-BUILDER-VERIFIER-LIFECYCLE';name='Builder and verifier reusable lifecycle';states='idle;building;built;verifying;accepted;failed;reset';transitions='begin;append/finalize;verify;latch error;reset';invariants='First error remains stable; reset removes prior-run state; accepted model is coherent';failure_policy='Return the first owned failure and prevent accepted partial output';oracle='Success/failure/reset sequence across repeated reuse';future_layer='unit';status='AUDITADO'}
)

$transversal | Export-Csv -LiteralPath (Join-Path $root 'transversal-contracts.csv') -NoTypeInformation -Encoding utf8
$errors | Export-Csv -LiteralPath (Join-Path $root 'error-taxonomy.csv') -NoTypeInformation -Encoding utf8
$stateMachines | Export-Csv -LiteralPath (Join-Path $root 'state-machines.csv') -NoTypeInformation -Encoding utf8
$stateMap | Export-Csv -LiteralPath (Join-Path $root 'state-machine-contract-map.csv') -NoTypeInformation -Encoding utf8

[pscustomobject]@{
    contracts = $contracts.Count
    transversal_rows = $transversal.Count
    error_rows = $errors.Count
    state_machines = $stateMachines.Count
    state_mappings = $stateMap.Count
    stateless_contracts = @($transversal | Where-Object state_models -eq 'STATELESS_OR_VALUE_CONTRACT').Count
} | ConvertTo-Json
