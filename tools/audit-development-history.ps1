param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputRoot,

    [string]$Revision = "development",
    [int]$ProgressIntervalSeconds = 30
)

$ErrorActionPreference = "Stop"

function Assert-PathOnDriveA
{
    param([string]$PathValue)

    $resolved = [System.IO.Path]::GetFullPath($PathValue)
    if (-not $resolved.StartsWith("A:\", [System.StringComparison]::OrdinalIgnoreCase))
    {
        throw "Audit paths must remain on A:. Rejected: $resolved"
    }
    return $resolved
}

function Invoke-Git
{
    param([string[]]$Arguments)

    $lines = @(& $script:GitPath -C $script:Repository @Arguments 2>&1)
    if ($LASTEXITCODE -ne 0)
    {
        throw "git $($Arguments -join ' ') failed: $($lines -join [Environment]::NewLine)"
    }
    return $lines
}

function Classify-Commit
{
    param(
        [string]$Subject,
        [string[]]$Parents
    )

    if ($Parents.Count -gt 1) { return "merge" }
    if ($Subject -match '^(Revert|revert)\b') { return "reversion" }
    if ($Subject -match '^(docs?|documentation)(\(|:|\s)') { return "documentation" }
    if ($Subject -match '^(tests?|testing)(\(|:|\s)') { return "test" }
    if ($Subject -match '^(fix|bugfix)(\(|:|\s)') { return "correction" }
    if ($Subject -match '^(refactor)(\(|:|\s)') { return "refactor" }
    if ($Subject -match '^(build|ci|infra|chore)(\(|:|\s)') { return "infrastructure" }
    if ($Subject -match '^(feat|feature)(\(|:|\s)') { return "functional" }
    if ($Subject -match '(milestone|baseline|release|freeze)') { return "milestone" }
    return "evidence-insufficient"
}

$script:Repository = Assert-PathOnDriveA $RepositoryRoot
$resolvedOutput = Assert-PathOnDriveA $OutputRoot
$script:GitPath = (Get-Command git -ErrorAction Stop).Source

$currentBranch = (Invoke-Git @("branch", "--show-current") | Select-Object -First 1)
if ($currentBranch -ne "development")
{
    throw "The active branch must be development. Current branch: $currentBranch"
}

$head = (Invoke-Git @("rev-parse", "$Revision^{commit}") | Select-Object -First 1)
$hashes = @(Invoke-Git @("rev-list", "--reverse", "--topo-order", $Revision))
$roots = @(Invoke-Git @("rev-list", "--max-parents=0", $Revision))
if ($hashes.Count -eq 0 -or $roots.Count -ne 1)
{
    throw "Expected a non-empty history with exactly one root."
}

$inventoryRoot = Join-Path $resolvedOutput "inventory"
New-Item -ItemType Directory -Path $inventoryRoot -Force | Out-Null
$inventoryPath = Join-Path $inventoryRoot "commits.jsonl"
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$writer = New-Object System.IO.StreamWriter($inventoryPath, $false, $utf8NoBom)

$mergeCount = 0
$reversionCount = 0
$emptyCount = 0
$nextProgress = [DateTime]::UtcNow.AddSeconds($ProgressIntervalSeconds)
$started = [DateTime]::UtcNow

try
{
    for ($index = 0; $index -lt $hashes.Count; ++$index)
    {
        $hash = $hashes[$index]
        $metadata = @(Invoke-Git @(
            "show",
            "-s",
            "--format=%H%n%P%n%T%n%an%n%ae%n%aI%n%cn%n%ce%n%cI%n%s%n%b",
            $hash
        ))
        if ($metadata.Count -lt 10)
        {
            throw "Incomplete metadata for commit $hash"
        }

        $parents = @()
        if (-not [string]::IsNullOrWhiteSpace($metadata[1]))
        {
            $parents = @($metadata[1].Split(' ', [System.StringSplitOptions]::RemoveEmptyEntries))
        }

        $changes = @(Invoke-Git @(
            "diff-tree",
            "--root",
            "-m",
            "--no-commit-id",
            "--name-status",
            "-r",
            "--find-renames",
            "--find-copies",
            $hash
        ))
        $isEmpty = $changes.Count -eq 0
        $classification = Classify-Commit -Subject $metadata[9] -Parents $parents
        if ($parents.Count -gt 1) { ++$mergeCount }
        if ($classification -eq "reversion") { ++$reversionCount }
        if ($isEmpty) { ++$emptyCount }

        $record = [ordered]@{
            index = $index + 1
            hash = $metadata[0]
            parents = $parents
            tree = $metadata[2]
            author_name = $metadata[3]
            author_email = $metadata[4]
            author_date = $metadata[5]
            committer_name = $metadata[6]
            committer_email = $metadata[7]
            committer_date = $metadata[8]
            subject = $metadata[9]
            body = (($metadata | Select-Object -Skip 10) -join "`n")
            initial_classification = $classification
            is_merge = $parents.Count -gt 1
            is_empty = $isEmpty
            changes = $changes
            audit_state = "INVENTORIED"
        }
        $writer.WriteLine(($record | ConvertTo-Json -Compress -Depth 6))

        if ([DateTime]::UtcNow -ge $nextProgress -or $index + 1 -eq $hashes.Count)
        {
            $elapsed = [DateTime]::UtcNow - $started
            Write-Output (
                "[HISTORY-AUDIT] phase=inventory processed={0} total={1} current={2} elapsed={3}" -f
                ($index + 1), $hashes.Count, $hash, $elapsed
            )
            $nextProgress = [DateTime]::UtcNow.AddSeconds($ProgressIntervalSeconds)
        }
    }
}
finally
{
    $writer.Dispose()
}

$inventoryHash = (Get-FileHash -LiteralPath $inventoryPath -Algorithm SHA256).Hash.ToLowerInvariant()
$manifest = [ordered]@{
    schema_version = 1
    branch = "development"
    revision = $head
    root = $roots[0]
    commits = $hashes.Count
    merges = $mergeCount
    reversions = $reversionCount
    empty_commits = $emptyCount
    inventory = "inventory/commits.jsonl"
    inventory_sha256 = $inventoryHash
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
}
$manifestPath = Join-Path $resolvedOutput "MANIFEST.json"
[System.IO.File]::WriteAllText(
    $manifestPath,
    ($manifest | ConvertTo-Json -Depth 5) + "`n",
    $utf8NoBom
)

$cursor = [ordered]@{
    schema_version = 1
    branch = "development"
    revision = $head
    status = "inventory_complete"
    inventoried = $hashes.Count
    audited = 0
    omitted = 0
    unknown = 0
    last_audited_index = 0
    last_audited_commit = $null
    next_index = 1
    next_commit = $hashes[0]
    next_action = "audit commit 1 from inventory/commits.jsonl"
}
$cursorPath = Join-Path $resolvedOutput "CURSOR.json"
[System.IO.File]::WriteAllText(
    $cursorPath,
    ($cursor | ConvertTo-Json -Depth 5) + "`n",
    $utf8NoBom
)

Write-Output "[HISTORY-AUDIT] manifest=$manifestPath"
Write-Output "[HISTORY-AUDIT] cursor=$cursorPath"
Write-Output "[HISTORY-AUDIT] inventory_sha256=$inventoryHash"
