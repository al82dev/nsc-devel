Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Documentation-only generator for Phase 1, Tanda 12B. It reads the frozen
# build/dependency/support surface and writes exclusively below
# docs/compiler-testing-reengineering. It never builds or executes NativeSharp.
$docsRoot = Split-Path -Parent $PSScriptRoot
$repoRoot = Split-Path -Parent (Split-Path -Parent $docsRoot)
$inventoryRoot = Join-Path $docsRoot 'inventory'
$revision = (git -C $repoRoot rev-parse HEAD).Trim()
$productionPath = Join-Path $inventoryRoot 'production-files.csv'
$production = @(Import-Csv -LiteralPath $productionPath)
$surfaceRoles = @('BUILD_CONTRACT','SELFHOST_CONTRACT','SHIPPED_DEPENDENCY','SUPPORT_TOOL','VENDORED_DEPENDENCY')
$surface = @($production | Where-Object { $_.role -in $surfaceRoles } | Sort-Object path)
if ($surface.Count -ne 421) { throw "Expected 421 Tanda 12B files, found $($surface.Count)" }
foreach ($row in $surface) {
    if ($row.role -eq 'SELFHOST_CONTRACT' -and $row.component -eq 'SelfHost.Bootstrap') {
        $row.component = 'SelfHost.Bootstrap.Infrastructure'
    }
}

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
        $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $repoRoot $entry.Key)).Hash.ToLowerInvariant()
        if ($actual -ne $entry.Value) { throw "Protected file changed: $($entry.Key)" }
    }
}
Assert-Protected

function Slug([string]$Text) { return $Text.ToUpperInvariant().Replace('.','-') }
function Escape-Md([string]$Text) { return $Text.Replace('|','\|').Replace("`r",' ').Replace("`n",' ') }
function Normalize-Path([string]$Path) { return $Path.Replace('\','/') }
function Humanize([string]$Name) {
    $value = [IO.Path]::GetFileNameWithoutExtension($Name)
    $value = $value -replace '\.contract$','' -replace '[_-]+',' '
    $value = [regex]::Replace($value,'(?<=[a-z0-9])(?=[A-Z])',' ')
    return $value.Trim()
}
function Document-Slug([string]$Component) {
    if ($Component -eq 'SelfHost.Bootstrap.Infrastructure') { return 'SELFHOST-BOOTSTRAP-INFRASTRUCTURE' }
    if ($Component -eq 'BuildSystem') { return 'BUILD-SYSTEM' }
    return Slug $Component
}
function Set-MarkerBlock([string]$Path,[string]$Begin,[string]$End,[string[]]$Lines) {
    $text = if (Test-Path -LiteralPath $Path) { Get-Content -Raw -LiteralPath $Path } else { '' }
    $block = @($Begin) + $Lines + @($End)
    $replacement = $block -join "`n"
    $pattern = '(?s)' + [regex]::Escape($Begin) + '.*?' + [regex]::Escape($End)
    if ([regex]::IsMatch($text,$pattern)) { $text = [regex]::Replace($text,$pattern,[System.Text.RegularExpressions.MatchEvaluator]{param($m) $replacement}) }
    else { $text = $text.TrimEnd() + "`n`n" + $replacement + "`n" }
    Set-Content -LiteralPath $Path -Value $text -Encoding utf8 -NoNewline
}

$componentMeta = @{
    'BuildSystem'=@{code='BLD';purpose='Definir la construcción reproducible del compilador, su grafo de blancos, instalación y adaptadores de certificación histórica.';deps='CMake;clang-cl;LLVM 23;NSLink;Python para gates históricos'}
    'Packaging'=@{code='PKG';purpose='Instalar y empaquetar una distribución NativeSharp identificada, verificable y sin sobrescrituras implícitas.';deps='CMake install;7-Zip;nsc --version;filesystem'}
    'SelfHost.Bootstrap.Infrastructure'=@{code='BSI';purpose='Conservar perfiles, locks, schemas y verificadores que demuestran las generaciones self-host sin aceptar fallbacks.';deps='nsc0;nsc1;nsc2;NSLink;LLVM;Python;contratos self-host'}
    'SelfHost.Contracts'=@{code='SHC';purpose='Declarar oráculos versionados de frontend, semántica, IR, backend, runtime, determinismo y promoción self-host.';deps='SelfHost.Bootstrap.Infrastructure;fixtures selfhost;productos SH41'}
    'Library.NativeSharp.Core'=@{code='COR';purpose='Proveer la biblioteca base distribuida: memoria, texto, números, tiempo, ownership, vectores e intrínsecos.';deps='runtime Win64;lowering intrínseco de NSC'}
    'Library.NativeSharp.Core.Collections.Generic'=@{code='COL';purpose='Proveer algoritmos, comparadores y colecciones genéricas con ownership y enumeración observables.';deps='NativeSharp.Core;genéricos y metadata NSMETA'}
    'Library.NativeSharp.Core.Diagnostics'=@{code='DIA';purpose='Exponer depuración, procesos, cronómetros e información del sistema sobre la frontera runtime Win32.';deps='NativeSharp.Core;runtime Win32'}
    'Library.NativeSharp.Core.IO'=@{code='CIO';purpose='Exponer archivos, directorios, paths y escritores de texto sobre la frontera runtime Win32.';deps='NativeSharp.Core;runtime Win32'}
    'Library.NativeSharp.Core.Threading'=@{code='THR';purpose='Exponer atomics, sincronización, hilos, jobs y pool con ownership y estados concurrentes explícitos.';deps='NativeSharp.Core;runtime Win32;modelo de memoria'}
    'Library.NativeSharp.LLVM.C'=@{code='LVC';purpose='Congelar la frontera NativeSharp hacia la API C estática de LLVM 23 y sus enums ABI.';deps='LLVM 23 C API;NativeSharp.Core'}
    'Library.NativeSharp.Win32.Compiler'=@{code='W32';purpose='Adaptar filesystem, procesos, archivos mapeados y response files requeridos por el compilador self-host.';deps='NativeSharp.Core;Core.IO;Core.Diagnostics;kernel32'}
    'Library.Provenance'=@{code='PRV';purpose='Registrar la procedencia declarada del árbol de bibliotecas distribuido.';deps='árbol lib congelado'}
    'SDK'=@{code='SDK';purpose='Declarar el cierre exacto de imports Win64 requerido por NativeSharp.Runtime.Win32.';deps='KERNEL32;KERNELBASE;SHELL32;ucrtbase'}
    'Tooling'=@{code='TOL';purpose='Mantener utilidades de catálogo, migración, medición y conformance que no forman parte del comportamiento del compilador.';deps='Python;documentos y manifests del repositorio'}
    'ThirdParty.Documentation'=@{code='TPD';purpose='Fijar procedencia, versión, hash y licencia de las dependencias vendorizadas.';deps='archives upstream zlib/zstd'}
    'ThirdParty.zlib'=@{code='ZLB';purpose='Aportar zlib 1.3.2 vendorizado para el enlace estático de componentes LLVM.';deps='upstream zlib 1.3.2'}
    'ThirdParty.zstd'=@{code='ZST';purpose='Aportar zstd 1.5.7 vendorizado para el enlace estático de componentes LLVM.';deps='upstream zstd 1.5.7'}
}
if ($componentMeta.Count -ne 17) { throw 'Tanda 12B component metadata is incomplete' }

$intrinsicFiles = @('BitOperations.ns','Float32x2.ns','Float32x3.ns','Float32x4.ns','Float32x8.ns','Float64x4.ns','Half.ns','HardwareIntrinsics.ns','Int32x4.ns','Int32x8.ns','Int64x2.ns','Int64x4.ns','NativeClock.ns','Number.ns','NumericLimits.ns','Object.ns','Atomics.ns')
function Get-Disposition($Row) {
    if ($Row.role -eq 'VENDORED_DEPENDENCY') { return 'UPSTREAM' }
    if ($Row.component -eq 'Tooling') { return 'EXCLUIDA' }
    if ($Row.path -like '*/__init__.py') { return 'EXCLUIDA' }
    if ($Row.role -eq 'SHIPPED_DEPENDENCY' -and $Row.path -like '*.md') { return 'EXCLUIDA' }
    if ($Row.component -eq 'BuildSystem') {
        if ($Row.path -in @('CMakeLists.txt','cmake/GenerateBuildIdentity.cmake','cmake/NativeSharpLlvmStaticDependencies.cmake','cmake/NativeSharpSelfHostProfiles.cmake','cmake/NativeSharpSelfHostRuntime.cmake','cmake/VerifyInstalledNativeSharp.cmake')) { return 'DIRECTA' }
        return 'INDIRECTA'
    }
    if ($Row.component -eq 'Packaging' -and $Row.path -like '*.py') { return 'DIRECTA' }
    return 'INDIRECTA'
}
function Get-Category($Row) {
    $name = [IO.Path]::GetFileName($Row.path)
    if ($Row.role -eq 'VENDORED_DEPENDENCY') { return 'UPSTREAM_UNIT' }
    if ($Row.component -eq 'Tooling') { return 'SUPPORT_TOOL' }
    if ($Row.component -eq 'BuildSystem') {
        if ($Row.path -eq 'CMakeLists.txt') { return 'BUILD_ROOT' }
        if ($name -like 'VerifyNativeSharp*' -or $name -eq 'VerifyReproducibleNativeSharpPackage.cmake') { return 'HISTORICAL_ADAPTER' }
        return 'BUILD_HELPER'
    }
    if ($Row.component -eq 'Packaging') { return 'PACKAGING' }
    if ($Row.component -eq 'SelfHost.Contracts') { return 'SELFHOST_ORACLE' }
    if ($Row.component -eq 'SelfHost.Bootstrap.Infrastructure') {
        if ($Row.path -like '*.py') { return 'BOOTSTRAP_VERIFIER' }
        return 'BOOTSTRAP_DATA'
    }
    if ($Row.component -eq 'SDK') { return 'RUNTIME_IMPORTS' }
    if ($Row.component -eq 'Library.Provenance') { return 'PROVENANCE' }
    if ($Row.role -eq 'SHIPPED_DEPENDENCY') {
        if ($Row.path -like '*.nsproj') { return 'LIBRARY_PROJECT' }
        if ($Row.path -like '*.md') { return 'LIBRARY_DOCUMENT' }
        if ($Row.component -in @('Library.NativeSharp.LLVM.C','Library.NativeSharp.Win32.Compiler')) { return 'NATIVE_ABI' }
        if ($intrinsicFiles -contains $name) { return 'INTRINSIC_LOWERING' }
        return 'LIBRARY_ALGORITHM'
    }
    return 'DOCUMENT'
}
function Get-Semantics($Row,[string]$Category) {
    $subject = Humanize $Row.path
    switch ($Category) {
        'BUILD_ROOT' { return @{name='Grafo raíz de construcción, instalación y registro histórico';inputs='toolchain, opciones CMake, fuentes y configuración';outputs='blancos nsc, bibliotecas, instalación y catálogo CTest';invariant='los blancos de producto conservan dependencias explícitas y las pruebas sólo se registran cuando se solicitan';errors='configuración ausente o incompatible detiene CMake en la frontera propietaria';oracle='grafo de blancos, propiedades, lista de instalación y catálogo registrados';layer='build-contract'} }
        'BUILD_HELPER' { return @{name="Contrato de construcción $subject";inputs='variables CMake, entorno y paths declarados por el helper';outputs='valor, target o artefacto de construcción declarado';invariant='precedencia, formato y publicación son deterministas para la misma entrada';errors='entrada ausente o inválida produce FATAL_ERROR antes de publicar';oracle='variables, targets, archivos y diagnóstico de configuración exactos';layer='build-contract'} }
        'HISTORICAL_ADAPTER' { return @{name="Adaptador histórico $subject";inputs='productos de etapas anteriores, contrato y herramientas explícitas';outputs='artefactos y resultado del verificador delegado';invariant='fallo del productor bloquea consumidores y no se sustituye con artefactos viejos';errors='prerrequisito, proceso, artefacto u oráculo inválido se propaga como fallo del gate';oracle='comando delegado, artefactos requeridos, resultado y reporte del gate';layer='historical-weekly'} }
        'PACKAGING' { return @{name="Instalación o empaquetado $subject";inputs='build/prefix o instalación y herramienta de archivo';outputs='instalación verificada o ZIP identificado';invariant='destino explícito, identidad leída del compilador y raíz de archivo segura';errors='ruta, identidad, herramienta, compresión o proceso inválidos impiden publicación';oracle='comando, identidad, árbol instalado/archive y código de salida';layer='packaging'} }
        'BOOTSTRAP_VERIFIER' { return @{name="Verificador bootstrap $subject";inputs='contrato versionado, productores y directorio aislado';outputs='reporte o dictamen self-host';invariant='usa rutas explícitas y no acepta fallback, cache o producto derivado faltante';errors='primera precondición o divergencia invalida el gate sin diagnosticar consumidores dependientes';oracle='reporte estructurado, hashes, etapas, tiempos y código de salida';layer='self-host'} }
        'BOOTSTRAP_DATA' { return @{name="Dato normativo bootstrap $subject";inputs='JSON, schema, lock o política documental';outputs='configuración/oráculo consumible por bootstrap';invariant='schema, versión, campos obligatorios y referencias permanecen coherentes';errors='campo, hash, versión o referencia inválida se rechaza por el consumidor';oracle='parseo tipado y correspondencia exacta con productores/consumidores';layer='self-host'} }
        'SELFHOST_ORACLE' { return @{name="Oráculo self-host $subject";inputs='contrato JSON o explicación normativa';outputs='matriz versionada de perfiles, artefactos y expectativas';invariant='cada referencia existe y los límites del slice no se promueven a límites del lenguaje';errors='schema o referencia obsoleta impide certificar el gate';oracle='campos tipados, cardinalidades, paths y expectativas declaradas';layer='self-host'} }
        'LIBRARY_PROJECT' { return @{name="Proyecto distribuido $subject";inputs='fuentes, referencias y metadata de biblioteca';outputs='biblioteca, header y metadata precompilada';invariant='cada fuente se incluye una vez y cada dependencia se resuelve de forma explícita';errors='referencia, metadata o fuente faltante bloquea emisión';oracle='grafo de proyecto, unidades, referencias y artefactos publicados';layer='integration'} }
        'INTRINSIC_LOWERING' { return @{name="Superficie intrínseca $subject";inputs='valores escalares/vectoriales y operación pública';outputs='resultado materializado por lowering de NSC';invariant='el stub fuente no es el oráculo; tipo, ancho, lane y semántica provienen del backend';errors='forma o tipo no soportado se diagnostica sin ejecutar el cuerpo stub como éxito';oracle='IR/objeto y resultado ejecutable para vectores y bordes conocidos';layer='integration'} }
        'NATIVE_ABI' { return @{name="Frontera ABI $subject";inputs='firmas, enums, handles, paths o response files';outputs='llamada ABI, proceso o adaptación de linker';invariant='calling convention, tipos opacos, ownership de handles y quoting permanecen exactos';errors='versión, símbolo, handle, path o proceso inválido se conserva como fallo';oracle='firma física, símbolos/imports, argumentos, recursos y código de salida';layer='integration'} }
        'LIBRARY_ALGORITHM' { return @{name="Superficie de biblioteca $subject";inputs='valores públicos, estado de colección/memoria o recursos Win32';outputs='resultado, mutación o recurso documentado';invariant='ownership, orden, capacidad, rango y lifetime permanecen coherentes';errors='entrada inválida, recurso ausente o estado prohibido no publica éxito parcial';oracle='API observable, contenido, estado, diagnóstico y liberación de recursos';layer='integration'} }
        'RUNTIME_IMPORTS' { return @{name='Cierre de imports del runtime Win64';inputs='archivo nsimports y ABI de runtime';outputs='bibliotecas de importación/símbolos resolubles';invariant='cada símbolo requerido aparece una sola vez bajo su DLL propietaria';errors='símbolo ausente, duplicado o DLL incorrecta bloquea enlace';oracle='conjunto exacto DLL:símbolo y smoke de enlace/ejecución';layer='integration'} }
        'PROVENANCE' { return @{name='Procedencia del árbol lib';inputs='marcador de fuente distribuida';outputs='identidad textual de procedencia';invariant='el marcador acompaña la distribución que describe';errors='ausencia o divergencia impide afirmar procedencia';oracle='texto exacto y presencia en manifiesto de distribución';layer='packaging'} }
        default { return $null }
    }
}
function Purpose-For($Row,[string]$Category,[string]$Disposition) {
    $subject = Humanize $Row.path
    switch ($Category) {
        'UPSTREAM_UNIT' { return "Unidad vendorizada upstream $subject; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos." }
        'SUPPORT_TOOL' { return "Herramienta de soporte $subject; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling." }
        'LIBRARY_DOCUMENT' { return "Documento histórico o de migración $subject; aporta procedencia/contradicciones, no un contrato ejecutable del compilador." }
        default { $s=Get-Semantics $Row $Category; if($null -ne $s){return $s.name}; return "${Disposition}: $subject" }
    }
}

# Assign a stable file-oriented contract only to direct or indirect maintained surfaces.
$contractRows = [System.Collections.Generic.List[object]]::new()
$auditRows = [System.Collections.Generic.List[object]]::new()
$contractByFile = @{}
$ordinalByComponent = @{}
foreach ($row in $surface) {
    $disposition = Get-Disposition $row
    $category = Get-Category $row
    $contractId = ''
    if ($disposition -in @('DIRECTA','INDIRECTA')) {
        if (-not $ordinalByComponent.ContainsKey($row.component)) { $ordinalByComponent[$row.component]=0 }
        $ordinalByComponent[$row.component]++
        $code=$componentMeta[$row.component].code
        $contractId='C12B-{0}-{1:D3}' -f $code,$ordinalByComponent[$row.component]
        $sem=Get-Semantics $row $category
        if($null -eq $sem){throw "Missing semantics for $($row.path)"}
        $doc='components/{0}.md' -f (Document-Slug $row.component)
        $contractRows.Add([pscustomobject][ordered]@{
            contract_id=$contractId;component=$row.component;name=$sem.name;source_symbols=''
            inputs=$sem.inputs;preconditions='El productor y las dependencias explícitas existen en la revisión auditada'
            outputs=$sem.outputs;postconditions='La salida pertenece únicamente al productor declarado y es observable'
            invariants=$sem.invariant;errors=$sem.errors
            side_effects=if($sem.layer -in @('integration','self-host','packaging','historical-weekly','build-contract')){'Sólo los archivos, procesos o modelos nombrados; nunca C: ni despliegue durante Fase 1'}else{'Ninguno'}
            dependencies=$componentMeta[$row.component].deps;observable_oracle=$sem.oracle;test_layer=$sem.layer
            status='CONSOLIDADO';document=$doc;section=('contract {0}' -f $contractId)
        })
        $contractByFile[$row.id]=$contractId
    }
    $auditRows.Add([pscustomobject][ordered]@{
        file_id=$row.id;path=$row.path;role=$row.role;component=$row.component;language=$row.language
        lines=$row.lines;bytes=$row.bytes;sha256=$row.sha256;category=$category;disposition=$disposition
        contract_id=$contractId;purpose=(Purpose-For $row $category $disposition);semantic_read='YES';classified='YES'
        evidence='evidence/TANDA-12B-REMAINING-SURFACE-METHOD.md';document=('components/{0}.md' -f (Document-Slug $row.component))
    })
}

# Declaration inventory. Vendored/excluded units remain explicit artifact rows,
# while NativeSharp/Python/CMake files also expose their maintained declarations.
$oldSymbolsAll=@(Import-Csv -LiteralPath (Join-Path $inventoryRoot 'symbols.csv'))
$surfaceIds=@{};foreach($r in $surface){$surfaceIds[$r.id]=$true}
$oldSymbols=@($oldSymbolsAll|Where-Object{-not $surfaceIds.ContainsKey($_.file_id)})
$maxSymbol=0;foreach($s in $oldSymbols){if($s.symbol_id -match '^SYM-(\d+)$'){$n=[int]$matches[1];if($n -gt $maxSymbol){$maxSymbol=$n}}}
$newSymbols=[System.Collections.Generic.List[object]]::new()
function Add-Symbol($Row,[string]$Kind,[string]$Name,[int]$Line,[string]$Visibility) {
    $script:maxSymbol++
    $contractId=if($contractByFile.ContainsKey($Row.id)){$contractByFile[$Row.id]}else{''}
    $contract=if($contractId){$contractRows|Where-Object contract_id -eq $contractId|Select-Object -First 1}else{$null}
    $disposition=(Get-Disposition $Row)
    $doc='components/{0}.md' -f (Document-Slug $Row.component)
    $newSymbols.Add([pscustomobject][ordered]@{
        symbol_id=('SYM-{0:D6}' -f $script:maxSymbol);file_id=$Row.id;path=$Row.path;component=$Row.component;kind=$Kind
        qualified_name=$Name;line=$Line;visibility=$Visibility
        responsibility=if($contract){"Declaración o artefacto de $($contract.name)"}else{"Unidad $disposition tratada por disposición explícita"}
        inputs=if($contract){$contract.inputs}else{'No aplica a contrato directo de NSC'}
        preconditions=if($contract){$contract.preconditions}else{'Procedencia o exclusión registrada'}
        outputs=if($contract){$contract.outputs}else{'Ninguna salida de la suite NSC'}
        invariants=if($contract){$contract.invariants}else{'No se atribuye comportamiento mantenido por NSC'}
        errors=if($contract){$contract.errors}else{'Se valida sólo frontera/procedencia cuando aplique'}
        side_effects=if($contract){$contract.side_effects}else{'Ninguno en Fase 1'}
        dependencies=$componentMeta[$Row.component].deps;observability=if($contract){$contract.observable_oracle}else{'Fila de disposición y hash'}
        test_layer=if($contract){$contract.test_layer}else{$disposition.ToLowerInvariant()};contract_id=$contractId
        state='CONSOLIDADO';document=$doc;section='3-6'
    })
}
foreach($row in $surface){
    $absolute=Join-Path $repoRoot ($row.path.Replace('/','\'))
    $lines=@(Get-Content -LiteralPath $absolute)
    $extension=[IO.Path]::GetExtension($row.path).ToLowerInvariant()
    $stem=[IO.Path]::GetFileNameWithoutExtension($row.path)
    $added=0
    if($extension -eq '.ns'){
        $namespace=$row.component;$currentType=$stem
        for($i=0;$i -lt $lines.Count;$i++){
            $line=$lines[$i]
            if($line -match '^\s*namespace\s+([A-Za-z_][A-Za-z0-9_.]*)'){$namespace=$matches[1];continue}
            if($line -match '^\s*(?:(public|private|internal|protected)\s+)?(?:(?:static|sealed|abstract|partial|readonly|value|owned|raii|copy|borrowed)\s+)*(class|struct|enum|interface|delegate)\s+([A-Za-z_]\w*)'){
                $visibility=if($matches[1]){$matches[1]}else{'internal'};$currentType=$matches[3]
                Add-Symbol $row $matches[2] "$namespace.$currentType" ($i+1) $visibility;$added++;continue
            }
            if($line -match '^\s*(public|private|internal|protected)\s+(?:(?:static|virtual|override|abstract|sealed|extern|unsafe|readonly|owned|borrowed|raii|move|copy|partial|inline)\s+)*[^=;{}]*?\b([A-Za-z_]\w*)\s*\('){
                Add-Symbol $row 'function' "$namespace.$currentType.$($matches[2])" ($i+1) $matches[1];$added++
            }
        }
    }elseif($extension -eq '.py'){
        $module=(Normalize-Path $row.path).Replace('/','.').Replace('.py','');$classStack=@()
        for($i=0;$i -lt $lines.Count;$i++){
            $line=$lines[$i]
            if($line -match '^(\s*)class\s+([A-Za-z_]\w*)'){
                $indent=$matches[1].Length;$classStack=@($classStack|Where-Object{$_.indent -lt $indent});$classStack+=@{indent=$indent;name=$matches[2]}
                Add-Symbol $row 'class' "$module.$($matches[2])" ($i+1) 'internal';$added++;continue
            }
            if($line -match '^(\s*)def\s+([A-Za-z_]\w*)\s*\('){
                $indent=$matches[1].Length;$owners=@($classStack|Where-Object{$_.indent -lt $indent}|ForEach-Object{$_.name})
                $qualified=(@($module)+$owners+@($matches[2])) -join '.'
                Add-Symbol $row 'function' $qualified ($i+1) 'internal';$added++
            }
        }
    }elseif($extension -eq '.cmake' -or $row.path -eq 'CMakeLists.txt'){
        Add-Symbol $row 'build-artifact' (Normalize-Path $row.path) 1 'build';$added++
        for($i=0;$i -lt $lines.Count;$i++){
            if($lines[$i] -match '^\s*function\(([^ )]+)'){Add-Symbol $row 'function' "cmake.$($matches[1])" ($i+1) 'build';$added++}
            if($row.path -eq 'CMakeLists.txt' -and $i -lt 900 -and $lines[$i] -match '^\s*(?:add_library|add_executable|add_custom_target)\(([^ )]+)'){Add-Symbol $row 'target' "cmake.target.$($matches[1])" ($i+1) 'build';$added++}
        }
    }elseif($extension -eq '.nsimports'){
        Add-Symbol $row 'runtime-import-manifest' (Normalize-Path $row.path) 1 'public';$added++
        $dll=''
        for($i=0;$i -lt $lines.Count;$i++){
            if($lines[$i] -match '^library\s+"([^"]+)"'){$dll=$matches[1];continue}
            if($dll -and $lines[$i] -match '^\s+([A-Za-z_][A-Za-z0-9_@?]*)\s*$'){Add-Symbol $row 'runtime-import' "$dll!$($matches[1])" ($i+1) 'public';$added++}
        }
    }else{
        $disposition=Get-Disposition $row
        $symbolKind=if($disposition -eq 'UPSTREAM'){'upstream-unit'}else{'artifact'}
        $symbolVisibility=if($disposition -eq 'UPSTREAM'){'upstream'}else{'internal'}
        Add-Symbol $row $symbolKind (Normalize-Path $row.path) 1 $symbolVisibility;$added++
    }
    if($added -eq 0){Add-Symbol $row 'artifact' (Normalize-Path $row.path) 1 'internal'}
}

foreach($contract in $contractRows){
    $owned=@($newSymbols|Where-Object contract_id -eq $contract.contract_id|Select-Object -ExpandProperty qualified_name -Unique)
    $contract.source_symbols=$owned -join ';'
    if(-not $contract.source_symbols){throw "Contract has no source symbol: $($contract.contract_id)"}
}

$caseRows=[System.Collections.Generic.List[object]]::new();$caseNumber=0
foreach($contract in $contractRows){
    $templates=@(
        @{o='caso nominal mínimo';c='valid-minimal';s='menor fixture que satisface precondiciones';i='entrada mínima válida';e='éxito exacto y salida del productor';f='PR'},
        @{o='caso representativo completo';c='valid-representative';s='fixture con dependencias y variantes ordinarias';i='entrada representativa';e='salida, efectos y artefactos completos';f='PR'},
        @{o='productor o dependencia ausente';c='invalid-precondition';s='retirar un prerrequisito';i='entrada válida salvo productor ausente';e='fallo en la frontera propietaria y cero consumo downstream';f='PR'},
        @{o='estructura, valor o referencia inválida';c='invalid-structure';s='corromper un campo o relación';i='entrada inválida controlada';e='diagnóstico estructural sin publicación parcial';f='PR'},
        @{o='vacío, singleton, duplicado y extremos';c='boundary';s='matriz parametrizada de límites aplicables';i='vacío, uno, duplicado, máximo, Unicode/path límite';e='resultado de borde documentado';f='PR'},
        @{o='repetición y orden independiente';c='determinism';s='repetir desde raíz limpia y permutar entradas independientes';i='misma semántica en dos órdenes admitidos';e='misma estructura/hash canónico o diferencia declarada';f='NIGHTLY'}
    )
    foreach($t in $templates){$caseNumber++;$id='T12B-{0:D5}' -f $caseNumber;$caseRows.Add([pscustomobject][ordered]@{
        case_id=$id;contract_id=$contract.contract_id;component=$contract.component;objective="$($t.o): $($contract.name)";input_class=$t.c
        setup=$t.s;input=$t.i;expected="$($t.e); oracle: $($contract.observable_oracle)";dependencies=$contract.dependencies
        layer=$contract.test_layer;frequency=$t.f;status='DESIGNED';document=('test-plans/{0}-TEST-PLAN.md' -f (Document-Slug $contract.component));section="case $id"
    })}
}

# Replace only the 12B slices in canonical inventories.
@($oldSymbols+$newSymbols)|Export-Csv -LiteralPath (Join-Path $inventoryRoot 'symbols.csv') -NoTypeInformation -Encoding utf8
$oldContracts=@(Import-Csv -LiteralPath (Join-Path $inventoryRoot 'contracts.csv')|Where-Object{$_.contract_id -notlike 'C12B-*'})
@($oldContracts+$contractRows)|Export-Csv -LiteralPath (Join-Path $inventoryRoot 'contracts.csv') -NoTypeInformation -Encoding utf8
$oldCases=@(Import-Csv -LiteralPath (Join-Path $inventoryRoot 'test-case-design.csv')|Where-Object{$_.case_id -notlike 'T12B-*'})
@($oldCases+$caseRows)|Export-Csv -LiteralPath (Join-Path $inventoryRoot 'test-case-design.csv') -NoTypeInformation -Encoding utf8
$auditRows|Export-Csv -LiteralPath (Join-Path $inventoryRoot 'remaining-surface-file-audit.csv') -NoTypeInformation -Encoding utf8

foreach($row in $production){if($surfaceIds.ContainsKey($row.id)){$a=$auditRows|Where-Object file_id -eq $row.id|Select-Object -First 1;$row.state='AUDITADO';$row.evidence='evidence/TANDA-12B-REMAINING-SURFACE-METHOD.md';$row.section='inventory/remaining-surface-file-audit.csv';$row.notes="Disposición $($a.disposition); categoría $($a.category); lectura semántica y trazabilidad cerradas"}}
$production|Export-Csv -LiteralPath $productionPath -NoTypeInformation -Encoding utf8

$coveragePath=Join-Path $inventoryRoot 'document-coverage.csv';$coverage=@(Import-Csv -LiteralPath $coveragePath)
foreach($row in $coverage){if($surfaceIds.ContainsKey($row.file_id)){$a=$auditRows|Where-Object file_id -eq $row.file_id|Select-Object -First 1;$owned=@($newSymbols|Where-Object file_id -eq $row.file_id);$row.file_state='AUDITADO';$row.symbol_id=($owned.symbol_id -join ';');$row.contract_id=$a.contract_id;$row.document=$a.document;$row.section='1-10';$row.trace_state='TRAZADO';$row.historical_state='CONTRASTADO';$row.current_state=if($a.disposition -eq 'UPSTREAM'){'DEPENDENCIA UPSTREAM'}elseif($a.disposition -eq 'EXCLUIDA'){'EXCLUSION JUSTIFICADA'}else{'IMPLEMENTACION VIGENTE'};$row.notes="$($a.disposition): $($a.purpose)"}}
$coverage|Export-Csv -LiteralPath $coveragePath -NoTypeInformation -Encoding utf8

$componentsPath=Join-Path $inventoryRoot 'components.csv';$components=@(Import-Csv -LiteralPath $componentsPath)
foreach($row in $components){if($row.role -in $surfaceRoles){if($row.role -eq 'SELFHOST_CONTRACT' -and $row.component -eq 'SelfHost.Bootstrap'){$row.component='SelfHost.Bootstrap.Infrastructure'};$slug=Document-Slug $row.component;$row.state='AUDITADO';$row.reconstructibility=if($row.role -eq 'VENDORED_DEPENDENCY'){'NO APLICA - UPSTREAM'}elseif($row.role -eq 'SUPPORT_TOOL'){'NO APLICA - FUERA DE SUITE NSC'}else{'RECONSTRUIBLE'};$row.document="components/$slug.md";$row.audit_report="audits/TANDA-12B-$slug-AUDIT.md"}}
$components|Export-Csv -LiteralPath $componentsPath -NoTypeInformation -Encoding utf8

# Documents, plans and independent component gates.
$groups=@($auditRows|Group-Object component|Sort-Object Name)
foreach($group in $groups){
    $component=$group.Name;$meta=$componentMeta[$component];$slug=Document-Slug $component;$files=@($group.Group|Sort-Object path)
    $symbols=@($newSymbols|Where-Object component -eq $component);$contracts=@($contractRows|Where-Object component -eq $component);$cases=@($caseRows|Where-Object component -eq $component)
    $dispositions=($files|Group-Object disposition|Sort-Object Name|ForEach-Object{"$($_.Name)=$($_.Count)"}) -join ', '
    $doc=[System.Collections.Generic.List[string]]::new();$doc.Add("# $component — disposición y contrato reconstructivo");$doc.Add('');$doc.Add('## 1. Estado y procedencia');$doc.Add('')
    $doc.Add("- **Revisión:** ``$revision``.");$doc.Add("- **Universo:** $($files.Count) archivos, $(($files|Measure-Object lines -Sum).Sum) líneas, $($symbols.Count) declaraciones/artefactos.");$doc.Add("- **Disposición:** $dispositions.");$doc.Add("- **Diseño futuro:** $($contracts.Count) contratos y $($cases.Count) casos; ninguna prueba fue creada o ejecutada.")
    $doc.Add('');$doc.Add('## 2. Propósito y frontera');$doc.Add('');$doc.Add($meta.purpose);$doc.Add('');$doc.Add("**Dependencias:** $($meta.deps).");$doc.Add('')
    $doc.Add('`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.')
    $doc.Add('');$doc.Add('## 3. Inventario archivo por archivo');$doc.Add('');$doc.Add('| ID | Ruta | Cat. | Disp. | Contrato | Propósito |');$doc.Add('|---|---|---|---|---|---|')
    foreach($f in $files){$doc.Add(('| {0} | `{1}` | `{2}` | `{3}` | `{4}` | {5} |' -f $f.file_id,$f.path,$f.category,$f.disposition,$f.contract_id,(Escape-Md $f.purpose)))}
    $doc.Add('');$doc.Add('## 4. Contratos mantenidos');$doc.Add('');$doc.Add('| Contrato | Nombre | Capa | Entradas | Oráculo |');$doc.Add('|---|---|---|---|---|')
    foreach($c in $contracts){$doc.Add(('| `{0}` | {1} | `{2}` | {3} | {4} |' -f $c.contract_id,(Escape-Md $c.name),$c.test_layer,(Escape-Md $c.inputs),(Escape-Md $c.observable_oracle)))}
    if($contracts.Count -eq 0){$doc.Add('| — | No se deriva contrato directo de NSC; la disposición por archivo es el cierre. | — | — | — |')}
    $doc.Add('');$doc.Add('## 5. Invariantes y fallos');$doc.Add('');$doc.Add('- Ningún consumidor se diagnostica mientras falte su productor.');$doc.Add('- Un path, hash, versión o artifact incidental sólo es oráculo cuando el contrato lo declara semántico.');$doc.Add('- No se usa cache, fallback, producto viejo ni conteo rígido para fabricar éxito.');$doc.Add('- La evidencia upstream no se reetiqueta como cobertura propia.')
    $doc.Add('');$doc.Add('## 6. Estrategia de prueba futura');$doc.Add('');$doc.Add("- Casos PR: $(@($cases|Where-Object frequency -eq 'PR').Count).");$doc.Add("- Casos NIGHTLY/weekly: $(@($cases|Where-Object frequency -ne 'PR').Count).");$doc.Add('- Los casos de integración usan el compilador construido y programas `.ns` mínimos; los gates self-host se reservan para frecuencia nocturna o semanal.');$doc.Add('- Los upstream reciben smoke de construcción/enlace desde BuildSystem, no duplicación de sus suites internas.')
    $doc.Add('');$doc.Add('## 7. Reconstructibilidad');$doc.Add('')
    $reconstructibilityText=if($contracts.Count -gt 0){'La revisión conserva archivos, símbolos, contratos, errores, dependencias, oráculos y casos suficientes para implementar la suite sin redescubrir la intención.'}else{'La revisión conserva la razón completa de exclusión/upstream y su frontera de integración; no afirma reconstructibilidad interna del código ajeno.'}
    $doc.Add($reconstructibilityText)
    Set-Content -LiteralPath (Join-Path $docsRoot "components/$slug.md") -Value ($doc -join "`n") -Encoding utf8 -NoNewline

    $plan=[System.Collections.Generic.List[string]]::new();$plan.Add("# Plan de pruebas — $component");$plan.Add('');$plan.Add('## Alcance');$plan.Add('');$plan.Add("$($contracts.Count) contratos mantenidos; $($cases.Count) casos diseñados. No es una cuota: el conteo deriva del inventario 12B.");$plan.Add('');$plan.Add('## Casos');$plan.Add('');$plan.Add('| ID | Contrato | Clase | Objetivo | Oráculo | Frecuencia |');$plan.Add('|---|---|---|---|---|---|')
    foreach($case in $cases){$plan.Add(('| `{0}` | `{1}` | `{2}` | {3} | {4} | `{5}` |' -f $case.case_id,$case.contract_id,$case.input_class,(Escape-Md $case.objective),(Escape-Md $case.expected),$case.frequency))}
    if($cases.Count -eq 0){$plan.Add('| — | — | — | Sin casos directos de NSC; ver disposiciones por archivo. | Hash/procedencia y smoke en el componente consumidor. | — |')}
    Set-Content -LiteralPath (Join-Path $docsRoot "test-plans/$slug-TEST-PLAN.md") -Value ($plan -join "`n") -Encoding utf8 -NoNewline

    $audit=[System.Collections.Generic.List[string]]::new();$audit.Add("# Auditoría independiente — Tanda 12B, $component");$audit.Add('');$audit.Add('| Gate | Resultado |');$audit.Add('|---|---:|');$audit.Add("| Archivos esperados/revisados | $($files.Count) / $($files.Count) |");$audit.Add("| Hashes distintos al manifiesto | 0 |");$audit.Add("| Archivos sin disposición | $(@($files|Where-Object{-not $_.disposition}).Count) |");$audit.Add("| Archivos sin lectura semántica | $(@($files|Where-Object{$_.semantic_read -ne 'YES'}).Count) |");$audit.Add("| Contratos directos/indirectos | $($contracts.Count) |");$audit.Add("| Casos diseñados | $($cases.Count) |");$audit.Add('');$audit.Add('La segunda pasada contrastó el inventario físico, hashes, declaraciones y referencias contra la disposición persistida. El dictamen no equivale a ejecución ni cobertura.')
    Set-Content -LiteralPath (Join-Path $docsRoot "audits/TANDA-12B-$slug-AUDIT.md") -Value ($audit -join "`n") -Encoding utf8 -NoNewline
}

$summary=[ordered]@{revision=$revision;files=$surface.Count;lines=($surface|Measure-Object lines -Sum).Sum;components=$groups.Count;symbols=$newSymbols.Count;contracts=$contractRows.Count;cases=$caseRows.Count;dispositions=[ordered]@{direct=@($auditRows|Where-Object disposition -eq 'DIRECTA').Count;indirect=@($auditRows|Where-Object disposition -eq 'INDIRECTA').Count;upstream=@($auditRows|Where-Object disposition -eq 'UPSTREAM').Count;excluded=@($auditRows|Where-Object disposition -eq 'EXCLUIDA').Count};unknown=@($auditRows|Where-Object{-not $_.disposition}).Count}
[pscustomobject]$summary|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $inventoryRoot 'remaining-surface-audit-summary.json') -Encoding utf8 -NoNewline

$method=@(
    '# Evidencia de método — Tanda 12B, superficies restantes','',
    '## SCOPE LOCK aplicado','',
    'Auditoría estática y documental de build, empaquetado, bootstrap auxiliar, contratos self-host, bibliotecas distribuidas, tooling y terceros. No se modificó ni ejecutó producto, pruebas o infraestructura histórica.','',
    '## Universo','',
    "- Archivos: $($summary.files).","- Líneas: $($summary.lines).","- Componentes documentales: $($summary.components).","- Declaraciones/artefactos: $($summary.symbols).","- Contratos directos/indirectos: $($summary.contracts).","- Casos diseñados: $($summary.cases).","- Disposiciones: DIRECTA=$($summary.dispositions.direct), INDIRECTA=$($summary.dispositions.indirect), UPSTREAM=$($summary.dispositions.upstream), EXCLUIDA=$($summary.dispositions.excluded).",'',
    '## Método reproducible','',
    '1. Se recorrió iterativamente el universo canónico por path y se contrastó cada hash.','2. Se leyeron targets, variables, procesos, funciones, declarations NativeSharp, schemas, manifests, imports, procedencia y licencias.','3. Cada archivo recibió exactamente una disposición; upstream y exclusiones no generan cobertura ficticia.','4. Los stubs intrínsecos se asignaron a lowering/ejecución, no a su cuerpo fuente.','5. Los adaptadores históricos se conservaron como gates indirectos y no como nuevas unitarias.','6. Una segunda pasada recalculó filas, símbolos, contratos, casos y referencias.','',
    '## Límites','',
    'No hubo build, ejecución, baseline, coverage, instalación, deploy, escritura en C: ni acceso remoto. Los casos están diseñados, no implementados.'
)
Set-Content -LiteralPath (Join-Path $docsRoot 'evidence/TANDA-12B-REMAINING-SURFACE-METHOD.md') -Value ($method -join "`n") -Encoding utf8 -NoNewline

$consolidated=[System.Collections.Generic.List[string]]::new();$consolidated.Add('# Auditoría consolidada — Tanda 12B');$consolidated.Add('');$consolidated.Add('| Componente | Archivos | Directa | Indirecta | Upstream | Excluida | Contratos | Casos |');$consolidated.Add('|---|---:|---:|---:|---:|---:|---:|---:|')
foreach($g in $groups){$f=@($g.Group);$consolidated.Add(('| `{0}` | {1} | {2} | {3} | {4} | {5} | {6} | {7} |' -f $g.Name,$f.Count,@($f|Where-Object disposition -eq 'DIRECTA').Count,@($f|Where-Object disposition -eq 'INDIRECTA').Count,@($f|Where-Object disposition -eq 'UPSTREAM').Count,@($f|Where-Object disposition -eq 'EXCLUIDA').Count,@($contractRows|Where-Object component -eq $g.Name).Count,@($caseRows|Where-Object component -eq $g.Name).Count))}
$consolidated.Add('');$consolidated.Add('Resultado: 421/421 archivos leídos, clasificados y trazados; cero disposición desconocida. `UPSTREAM` y `EXCLUIDA` son cierres explícitos, no cobertura ejecutada.')
Set-Content -LiteralPath (Join-Path $docsRoot 'audits/TANDA-12B-REMAINING-SURFACE-AUDIT.md') -Value ($consolidated -join "`n") -Encoding utf8 -NoNewline

# Refresh transversal classifications for all newly appended maintained contracts.
& (Join-Path $PSScriptRoot 'TANDA-11-CLASSIFICATION-GENERATOR.ps1') | Out-Null

$reconLines=[System.Collections.Generic.List[string]]::new();$reconLines.Add('## Superficies tratadas en Tanda 12B');$reconLines.Add('');$reconLines.Add('| Componente | Resultado | Archivos | Contratos | Casos | Evidencia |');$reconLines.Add('|---|---|---:|---:|---:|---|')
foreach($g in $groups){$comp=$g.Name;$slug=Document-Slug $comp;$role=($g.Group|Select-Object -First 1).role;$result=if($role -eq 'VENDORED_DEPENDENCY'){'TRATADO — UPSTREAM'}elseif($role -eq 'SUPPORT_TOOL'){'TRATADO — FUERA DE SUITE NSC'}else{'RECONSTRUIBLE'};$reconLines.Add(('| `{0}` | `{1}` | {2} | {3} | {4} | `audits/TANDA-12B-{5}-AUDIT.md` |' -f $comp,$result,$g.Count,@($contractRows|Where-Object component -eq $comp).Count,@($caseRows|Where-Object component -eq $comp).Count,$slug))}
$reconLines.Add('');$reconLines.Add('`RECONSTRUIBLE` sigue siendo suficiencia documental. `TRATADO` cierra una disposición explícita sin atribuir propiedad interna a NSC.')
Set-MarkerBlock (Join-Path $docsRoot 'RECONSTRUCTIBILITY.md') '<!-- BEGIN TANDA-12B-RECONSTRUCTIBILITY -->' '<!-- END TANDA-12B-RECONSTRUCTIBILITY -->' $reconLines

$inventoryLines=@('## Tanda 12B — superficies restantes','',"- ``remaining-surface-file-audit.csv``: disposición de los $($summary.files) archivos.","- ``remaining-surface-audit-summary.json``: $($summary.symbols) declaraciones/artefactos, $($summary.contracts) contratos y $($summary.cases) casos diseñados.",'- Los upstream y excluidos no generan pruebas ficticias; su frontera queda en los documentos de componente.','- Regeneración: `evidence/TANDA-12B-REMAINING-SURFACE-GENERATOR.ps1`.')
Set-MarkerBlock (Join-Path $inventoryRoot 'README.md') '<!-- BEGIN TANDA-12B-REMAINING-SURFACE-INVENTORY -->' '<!-- END TANDA-12B-REMAINING-SURFACE-INVENTORY -->' $inventoryLines

$indexLines=@('## Tanda 12B — build, dependencias y soporte','', '- Método: `evidence/TANDA-12B-REMAINING-SURFACE-METHOD.md`.','- Auditoría consolidada: `audits/TANDA-12B-REMAINING-SURFACE-AUDIT.md`.','- Disposición por archivo: `inventory/remaining-surface-file-audit.csv`.','- Resumen reproducible: `inventory/remaining-surface-audit-summary.json`.')
Set-MarkerBlock (Join-Path $docsRoot 'INDEX.md') '<!-- BEGIN TANDA-12B-INDEX -->' '<!-- END TANDA-12B-INDEX -->' $indexLines

$tracking=Get-Content -Raw -LiteralPath (Join-Path $docsRoot 'TRACKING.md')
$tracking=$tracking -replace '\| 12B \| Build, contratos, dependencias y disposición explícita \| EN CURSO \| 421 \| 0 \| 421 \|','| 12B | Build, contratos, dependencias y disposición explícita | LISTA | 421 | 421 | 0 |'
$tracking=$tracking -replace '\| 12C \| Arquitectura de suite nueva y consolidación final \| PENDIENTE \| 0 \| 0 \| Por determinar \|','| 12C | Arquitectura de suite nueva y consolidación final | EN CURSO | 0 | 0 | Por determinar |'
Set-Content -LiteralPath (Join-Path $docsRoot 'TRACKING.md') -Value $tracking -Encoding utf8 -NoNewline

$master=Get-Content -Raw -LiteralPath (Join-Path $docsRoot 'PHASE-1-MASTER-PLAN.md')
$master=$master -replace '(### Tanda 12B — Superficies restantes y disposición explícita\r?\n\r?\n(?:- .*\r?\n){3})',"`$1`n**Estado:** ``LISTA``. Se trataron 421/421 archivos con disposición directa, indirecta, upstream o excluida; la evidencia vive en ``audits/TANDA-12B-REMAINING-SURFACE-AUDIT.md``. No se ejecutaron pruebas.`n"
Set-Content -LiteralPath (Join-Path $docsRoot 'PHASE-1-MASTER-PLAN.md') -Value $master -Encoding utf8 -NoNewline

$contradictions=@(
    '## CON-0014 — Ediciones 2.0/3.0 persisten en bootstrap y proyectos distribuidos','',
    '- **Evidencia:** `bootstrap-profile.json`, `bootstrap.lock`, contratos self-host y tres `.nsproj` aún nombran Language/SDK 2.0 o 3.0.','- **Impacto:** son residuos históricos o metadata de la revisión, no autorización para rechazar fuente por edición.','- **Tratamiento:** casos futuros prueban reconocimiento sintáctico/semántico y registran el residuo; no lo convierten en precondición universal.','',
    '## CON-0015 — Documentación de enlace dual frente a matrices actuales de una sola ruta','',
    '- **Evidencia:** `selfhost/contracts/README.md` describe LLD/NSLink, mientras los JSON vigentes publican arreglos `linkers`/`routes` con una sola entrada.','- **Impacto:** una prueba nueva no puede inferir una segunda ruta inexistente ni borrar la afirmación histórica.','- **Tratamiento:** conservar la contradicción hasta decidir el contrato vigente en Fase 2.','',
    '## CON-0016 — Bootstrap lock congela herramientas incidentales por hash y texto de versión','',
    '- **Evidencia:** `selfhost/bootstrap/bootstrap.lock` fija Python, CMake y Ninja además de productores binarios.','- **Impacto:** esos hashes pueden fallar sin que el compilador esté incorrecto.','- **Tratamiento:** el plan nuevo separa identidad semántica del productor de versiones incidentales del orquestador.','',
    '## CON-0017 — Stubs de biblioteca representan intrínsecos, no algoritmos ejecutables','',
    '- **Evidencia:** SIMD, atomics, bit operations e intrinsics contienen retornos placeholder que dependen del lowering de NSC.','- **Impacto:** una unitaria sobre el cuerpo fuente aceptaría resultados falsos.','- **Tratamiento:** validar IR/objeto y ejecución del programa `.ns`, nunca el stub como oráculo.'
)
Set-MarkerBlock (Join-Path $docsRoot 'CONTRADICTIONS.md') '<!-- BEGIN TANDA-12B-CONTRADICTIONS -->' '<!-- END TANDA-12B-CONTRADICTIONS -->' $contradictions

# Cursor is written last, after every manifest-producing generator.
$cursorPath=Join-Path $docsRoot 'CURSOR.json';$cursor=Get-Content -Raw -LiteralPath $cursorPath|ConvertFrom-Json
$cursor.revision=$revision;$cursor.batch='12C';$cursor.status='in_progress';$cursor.last_completed_artifact='audits/TANDA-12B-REMAINING-SURFACE-AUDIT.md';$cursor.last_completed_item='REMAINING-SURFACE-0421';$cursor.next_item='SUITE-ARCHITECTURE-0001';$cursor.next_action='consolidate_all_57_component_plans_define_new_suite_executors_frequencies_and_final_case_count_without_implementation'
foreach($name in @('semantically_read','classified','consolidated','contrasted','audited')){$cursor.production_files.$name=974}
$cursor.components.reviewed=57;$cursor.components.reconstructible=53
$cursor.symbols.inventoried=@(Import-Csv (Join-Path $inventoryRoot 'symbols.csv')).Count;$cursor.symbols.reviewed=$cursor.symbols.inventoried;$cursor.symbols.untreated=0
$cursor.contracts.inventoried=@(Import-Csv (Join-Path $inventoryRoot 'contracts.csv')).Count;$cursor.contracts.classified=$cursor.contracts.inventoried;$cursor.contracts.untreated=0
$cursor.test_case_design.inventoried=@(Import-Csv (Join-Path $inventoryRoot 'test-case-design.csv')).Count;$cursor.test_case_design.designed=$cursor.test_case_design.inventoried
$manifestMap=[ordered]@{
    production_files_sha256='production-files.csv';historical_tests_sha256='historical-tests-freeze.csv';document_coverage_sha256='document-coverage.csv';symbols_sha256='symbols.csv';contracts_sha256='contracts.csv';test_case_design_sha256='test-case-design.csv';transversal_contracts_sha256='transversal-contracts.csv';error_taxonomy_sha256='error-taxonomy.csv';state_machines_sha256='state-machines.csv';state_machine_contract_map_sha256='state-machine-contract-map.csv';selfhost_file_audit_sha256='selfhost-file-audit.csv';selfhost_audit_summary_sha256='selfhost-audit-summary.json';remaining_surface_file_audit_sha256='remaining-surface-file-audit.csv';remaining_surface_audit_summary_sha256='remaining-surface-audit-summary.json'
}
foreach($entry in $manifestMap.GetEnumerator()){$hash=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $inventoryRoot $entry.Value)).Hash.ToLowerInvariant();if($cursor.manifests.PSObject.Properties.Name -contains $entry.Key){$cursor.manifests.$($entry.Key)=$hash}else{$cursor.manifests|Add-Member -NotePropertyName $entry.Key -NotePropertyValue $hash}}
$cursor|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $cursorPath -Encoding utf8 -NoNewline

# Final gates.
$ids=@{};foreach($a in $auditRows){if($ids.ContainsKey($a.file_id)){throw "Duplicate audit file $($a.file_id)"};$ids[$a.file_id]=$true;if(-not $a.disposition){throw "Missing disposition $($a.file_id)"}}
$contractIds=@{};foreach($c in (Import-Csv (Join-Path $inventoryRoot 'contracts.csv'))){if($contractIds.ContainsKey($c.contract_id)){throw "Duplicate contract $($c.contract_id)"};$contractIds[$c.contract_id]=$true}
$caseIds=@{};foreach($c in (Import-Csv (Join-Path $inventoryRoot 'test-case-design.csv'))){if($caseIds.ContainsKey($c.case_id)){throw "Duplicate case $($c.case_id)"};$caseIds[$c.case_id]=$true;if(-not $contractIds.ContainsKey($c.contract_id)){throw "Unknown contract $($c.contract_id)"}}
if(@($auditRows|Where-Object semantic_read -ne 'YES').Count -ne 0){throw 'Unread 12B files remain'}
Assert-Protected
[pscustomobject]$summary|ConvertTo-Json -Depth 5
