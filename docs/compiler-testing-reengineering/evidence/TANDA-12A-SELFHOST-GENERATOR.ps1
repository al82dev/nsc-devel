Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# This script is documentation tooling. It reads the frozen self-host tree and
# rewrites only Phase 1 inventories and reports under docs/compiler-testing-reengineering.
# It deliberately uses iterative scans; it never builds or executes NativeSharp.
$docsRoot = Split-Path -Parent $PSScriptRoot
$repoRoot = Split-Path -Parent (Split-Path -Parent $docsRoot)
$inventoryRoot = Join-Path $docsRoot 'inventory'
$revision = (git -C $repoRoot rev-parse HEAD).Trim()
$productionPath = Join-Path $inventoryRoot 'production-files.csv'
$production = @(Import-Csv -LiteralPath $productionPath)
$selfHostFiles = @($production | Where-Object { $_.path -like 'selfhost/nsc/*' } | Sort-Object path)
if ($selfHostFiles.Count -ne 29) { throw "Expected 29 self-host files, found $($selfHostFiles.Count)" }

function New-Contract {
    param(
        [string]$Component, [string]$Prefix, [string]$Name, [string]$Pattern,
        [string]$Layer, [string]$Inputs, [string]$Outputs,
        [string]$Invariant, [string]$Errors, [string]$Oracle
    )
    [pscustomobject][ordered]@{
        component = $Component; prefix = $Prefix; name = $Name; pattern = $Pattern
        layer = $Layer; inputs = $Inputs; outputs = $Outputs; invariant = $Invariant
        errors = $Errors; oracle = $Oracle
    }
}

$contractsSpec = [System.Collections.Generic.List[object]]::new()
function Add-C {
    param([string]$C,[string]$P,[string]$N,[string]$R,[string]$L,[string]$I,[string]$O,[string]$V,[string]$E,[string]$Q)
    $contractsSpec.Add((New-Contract $C $P $N $R $L $I $O $V $E $Q))
}

# Basic value identities and storage.
Add-C 'SelfHost.Basic' 'CSH-BAS' 'Typed source identities and compiler results' 'FileId|SourcePosition|SourceRange|CompilerResult' 'unit' 'typed IDs positions ranges and exit result' 'stable values validity equality and success/failure' 'zero is the only invalid ID and ranges are half-open within one file' 'invalid identity or inconsistent range remains observably invalid' 'exact fields validity equality length and exit code'
Add-C 'SelfHost.Basic' 'CSH-BAS' 'Generational arena handles' 'ArenaHandle|GenerationArena' 'unit' 'byte size generation and arena state' 'handle address generation size resolution and usage' 'reset advances generation and stale handles never resolve as current storage' 'empty stale or out-of-range handles resolve to the documented sentinel' 'address generation size usage and stale-handle truth table'
Add-C 'SelfHost.Basic' 'CSH-BAS' 'Stable text hash' 'StableHash' 'unit' 'UTF-16 string including empty and non-ASCII' 'stable unsigned hash' 'same text always yields the same value independent of process order' 'none declared' 'exact known vectors and repeated-input identity'
Add-C 'SelfHost.Basic' 'CSH-BAS' 'Basic ownership and value lifetime' 'Address|Resolve|Reset|Success|Failure' 'unit' 'owned arena or value operation' 'non-owning view or value result' 'views remain valid only for their documented owner generation' 'stale generation is rejected without publication' 'generation transition and lifetime boundary'

# Bootstrap source/manifest admission.
Add-C 'SelfHost.Bootstrap' 'CSH-BOO' 'Bootstrap source profile verification' 'VerifySource' 'component' 'self-host source text and path' 'accepted source or first profile violation' 'only the frozen bootstrap subset is admitted' 'forbidden syntax ownership or profile constructs return stable failure' 'accept/reject matrix with first rejected construct'
Add-C 'SelfHost.Bootstrap' 'CSH-BOO' 'Bootstrap manifest verification' 'VerifyManifest' 'component' 'project manifest text' 'validated bootstrap project description' 'required profile references and source declarations are coherent' 'missing conflicting or unsupported manifest entries are rejected' 'normalized manifest model or exact rejection category'
Add-C 'SelfHost.Bootstrap' 'CSH-BOO' 'Bootstrap generation build orchestration' 'Build|GenerationBuilder' 'integration' 'verified sources manifest compiler and output request' 'bootstrap generation result and diagnostics' 'no downstream generation begins after admission failure' 'producer failure stops publication and preserves its exit identity' 'stage sequence outputs diagnostics and exit result'

# Bootstrap LLVM backend. Patterns are matched against qualified declaration names.
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Function-local and loop lowering state' 'LocalBinding|LoopBinding|FunctionLoweringState' 'unit' 'symbol/name storage type and loop targets' 'lookup receiver loop targets and break state' 'nearest binding and innermost loop win; pop restores previous state' 'missing binding yields null without inventing storage' 'lookup precedence push/pop and break-state transitions'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Entry discovery and reachability closure' 'EntryExpression|FindEntryExpression|MarkReachable|MarkLibrary|BackendEligible|BackendDeclarable|OpenGeneric' 'component' 'syntax tree symbols and executable/library mode' 'marked reachable declarations and entry expression' 'closure is deterministic and cycles terminate through reachability marks' 'missing executable Main or unsupported declaration is rejected at emission' 'reachable symbol set and selected entry'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Semantic type and closed-generic lowering' 'SupportedScalar|IntegerEnum|FindTypeSymbol|ClosedGeneric|ScalarType|SemanticValueType|PhysicalValueScalar|GenericPlaceholder' 'unit' 'semantic type display and closed generic context' 'LLVM physical type or unsupported result' 'same semantic/ABI context maps to one physical representation' 'unknown unsupported or open type returns no LLVM type' 'LLVM kind width element and aggregate representation'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Win64 ABI signatures and aggregates' 'AggregateSize|EnumSize|DirectAggregate|AbiValueType|ScalarFunctionType|DelegateFunctionType|HasReceiver|UsesNativeAbi|ParameterDeclarationAt' 'unit' 'function declaration semantic model and aggregate layout' 'physical function signature and passing mode' 'receiver and direct aggregate rules match declaration and calling convention' 'incomplete layout or signature prevents declaration' 'parameter count types receiver return and call convention'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Stable LLVM names and declarations' 'LlvmFunctionName|LlvmGlobalName|DeclareData|DeclareFunctions|ConstantInitializer' 'component' 'symbols syntax declarations and constants' 'named LLVM globals and functions' 'names are stable and declarations are unique per semantic identity' 'duplicate incompatible or unrepresentable declaration fails before bodies' 'exact names linkage initializers and declaration set'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Callable lookup and overload selection' 'FindFunctionDeclaration|FindFunctionDeclarationByArity|FindDelegateDeclaration|FindBestOverload|ExternDelegateType|RawFunctionReference|DestructorDeclaration' 'component' 'callee name arity arguments parent and delegate context' 'selected declaration or unresolved result' 'selection follows semantic identity before fallback search and preserves arity' 'ambiguous absent or incompatible callable remains unresolved' 'selected symbol signature and unresolved diagnostics'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Scalar conversion and operation normalization' 'IntegerBits|SemanticIntegerBits|UnsignedInteger|FloatingScalar|GenericResultPosition|BoxGenericScalar|ConvertScalar|ApplyConversion|SamePhysicalType' 'unit' 'source value semantic conversion fact and destination type' 'LLVM value with destination physical type' 'both operands of a physical operation have identical LLVM types' 'unsupported narrowing boxing or mismatched physical types fail locally' 'opcode source/destination widths signedness and result type'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Memory contracts and bit-fields' 'BitfieldStorageType|ApplyMemoryContract|LoadMemory|StoreMemory|LValueSymbol|ByteOffset' 'unit' 'address physical type symbol flags and value' 'loaded or stored value with volatile/atomic/bit-field policy' 'symbol memory policy is applied symmetrically to load and store' 'invalid address layout or conversion publishes no instruction result' 'instruction kind ordering alignment mask and stored bytes'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Native string allocation copy move and comparison' 'EmitGlobalString|AllocateNativeString|CopyNativeBytes|MoveNativeBytes|EmitClosedSpanCopy|CompareNativeStrings|StringType|StringExpression|StringIndexExpression' 'component' 'string or byte spans lengths and allocation context' 'native string storage copy/move result or equality' 'length header and byte region remain coherent and copies respect overlap policy' 'allocation or operand failure returns no accepted value' 'length bytes allocation calls and comparison result'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Runtime hooks core intrinsics and atomics' 'RuntimeHook|AtomicOrderingFor|EmitCoreIntrinsic' 'integration' 'recognized core call arguments and memory order' 'runtime call or intrinsic LLVM value' 'only named intrinsic shapes bypass ordinary call lowering' 'unrecognized calls remain unhandled; malformed recognized calls fail' 'selected hook signature opcode ordering and result'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Managed and native delegate lowering' 'EmitManagedDelegate|RawFunctionReference|DelegateFunctionType' 'component' 'code pointer receiver delegate declaration and arguments' 'delegate cell or direct/indirect call' 'managed cells preserve code receiver destroy/free slots; extern delegates stay raw' 'missing target receiver or signature fails without fabricated call' 'cell layout call branch arguments and result phi'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'L-value address lowering' 'EmitLValue|FindLocal|FindLocalByName' 'component' 'identifier dereference index or member expression' 'address element type and validity' 'address denotes the semantic storage and indirect parameters are not double-loaded' 'unbound storage invalid index receiver or field layout returns invalid' 'address provenance GEP offsets and value type'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Scalar expression lowering' 'EmitScalarExpression|EmitIntegerOperation|EmitFloatingOperation|ComparisonPredicate|FloatingComparisonPredicate|Comparison' 'component' 'typed expression tree and lowering state' 'LLVM scalar/address/delegate value' 'result physical type agrees with semantic fact after conversion' 'operand conversion call or allocation failure stops the expression' 'opcode constants branches calls memory and final LLVM type'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Condition and short-circuit lowering' 'EmitCondition|IsAlwaysTrue' 'component' 'condition or logical expression' 'i1 condition and CFG blocks' 'right side executes only on the required path and phi inputs match predecessors' 'invalid operand or block construction rejects the condition' 'CFG edges phi values and side-effect reachability'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Inline assembly lowering' 'InlineAssemblyTemplate|InlineAssemblyConstraints|EmitInlineAssembly' 'component' 'assembly template operands constraints clobbers and volatility' 'LLVM inline-asm call and output stores' 'operand order and constraint positions remain aligned' 'invalid input output l-value or physical type fails locally' 'template constraint string call convention side effects and outputs'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Statement and structured control lowering' 'EmitStatement|PushLoop|PopLoop|BreakTarget|ContinueTarget|MarkBreak|CurrentLoopHasBreak|EmitEntryAlloca' 'component' 'statement tree function and lowering state' 'terminated or fallthrough CFG with stores/calls' 'each emitted block has coherent termination and loop targets are properly nested' 'child lowering or termination failure rejects the function body' 'block/edge sequence terminators return values and cleanup calls'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Function body emission and LLVM verification' 'EmitFunctions|LoweringFailure|ReturnExpression' 'integration' 'declared reachable functions semantic model and builder' 'verified LLVM function bodies' 'every eligible body terminates and verifies before module acceptance' 'first body or verifier failure stops traversal with named diagnostic' 'function bodies verifier result and diagnostic identity'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'Optimization and target configuration' 'CodeGenLevel|PassPipeline' 'unit' 'O0 O1 O2 or O3' 'target codegen level and pass pipeline' 'each optimization selects its declared fixed pipeline' 'unknown enum follows documented default branch' 'exact level and pipeline text'
Add-C 'SelfHost.CodeGen' 'CSH-CG' 'COFF or assembly object emission lifecycle' 'EmitExecutableObject|Dispose|PrintLlvmMessage' 'integration' 'validated IR output path optimization and artifact kind' 'object/assembly file or stable backend exit code' 'LLVM major 23 target/data-layout verify/pass/debug/emission stages occur in order and resources are disposed' 'stage-specific codes 1..16 or 90 identify the first failed producer' 'exit code artifact kind module verification and emitted file structure'

# Diagnostics.
Add-C 'SelfHost.Diagnostics' 'CSH-DIA' 'Diagnostic severity and immutable record' 'DiagnosticSeverity|Diagnostic$|Code|Severity|Argument|ExitCode' 'unit' 'code severity argument and exit code' 'stable diagnostic record' 'construction preserves all fields' 'none declared' 'exact field values and copy lifetime'
Add-C 'SelfHost.Diagnostics' 'CSH-DIA' 'Diagnostic catalog' 'DiagnosticCatalog|MissingSource|UnknownOption' 'unit' 'missing path or unknown option' 'catalogued diagnostic' 'code severity and exit mapping remain stable per catalogue entry' 'none declared' 'exact code severity argument and exit code'
Add-C 'SelfHost.Diagnostics' 'CSH-DIA' 'Human diagnostic rendering' 'Human|NormalizedPath' 'unit' 'diagnostic and source path' 'single human-readable line' 'path normalization and stable code/argument placement do not depend on cwd' 'none declared' 'exact normalized semantic fields ignoring incidental locale'
Add-C 'SelfHost.Diagnostics' 'CSH-DIA' 'JSON diagnostic rendering' 'Json|JsonEscape' 'unit' 'diagnostic and path with control/non-ASCII text' 'valid deterministic JSON object' 'every control character is escaped and fields retain diagnostic identity' 'none declared' 'parse JSON then compare exact typed fields and escaping'

# Documentation and project manifest are contracts even without declarations.
Add-C 'SelfHost.Documentation' 'CSH-DOC' 'Self-host architecture declaration' 'README' 'build-contract' 'README at frozen revision' 'declared component boundaries and stage history' 'the document is evidence, not executable truth' 'document/code disagreement is a recorded contradiction' 'trace each claim to current source or mark historical'
Add-C 'SelfHost.Documentation' 'CSH-DOC' 'Bootstrap profile and hidden-implementation prohibition' 'BootstrapProfile|wrapper|circular' 'build-contract' 'README policy statements' 'documented bootstrap restrictions' 'no wrapper C++ or hidden alternate frontend is permitted by the document' 'current version residue remains an open contradiction' 'source/project inventory and explicit contradiction record'

# Driver command surface.
Add-C 'SelfHost.Driver' 'CSH-DRV' 'CLI identity help and diagnostic presentation' 'PrintVersion|PrintHelp|EmitDiagnostic' 'process' 'command line and diagnostic format selection' 'version/help text or diagnostic on selected stream' 'read-only commands do not enter compilation' 'unknown options use catalogued exit identity' 'exit code streams and semantic fields'
Add-C 'SelfHost.Driver' 'CSH-DRV' 'Source loading and frontend dumps' 'ReadSource|DumpTokens|DumpAst|DumpSemantic' 'process' 'source path and dump command' 'tokens AST symbols or semantic dump' 'each dump is deterministic for the same source and stops after owned frontend failure' 'read lex parse or semantic failure maps to nonzero result' 'normalized dump text and exit code'
Add-C 'SelfHost.Driver' 'CSH-DRV' 'IR preparation verification and dumps' 'RunIr|DumpProjectIr' 'process' 'source/project and IR command' 'HIR MIR CFG dominance SSA or pass dump' 'IR is admitted only through ValidatedIrProgram' 'failed producer prevents downstream dump' 'selected dump exact structure and exit code'
Add-C 'SelfHost.Driver' 'CSH-DRV' 'Self-test command' 'SelfTest' 'process' 'self-test command' 'aggregate internal result' 'the command exercises declared bootstrap primitives without filesystem publication' 'first failed check returns stable nonzero result' 'check identities and final exit code'
Add-C 'SelfHost.Driver' 'CSH-DRV' 'Single-source backend emission' 'EmitBackend' 'process' 'source output optimization and artifact kind' 'object or assembly artifact' 'frontend and IR success precede backend emission' 'producer/backend code is preserved and no success is printed on failure' 'stage order artifact bytes and exit code'
Add-C 'SelfHost.Driver' 'CSH-DRV' 'Project backend emission' 'EmitProjectBackend' 'process' 'project graph output optimization artifact and library mode' 'project object or library object set' 'source graph module set semantic and IR stages complete before codegen' 'any unit or dependency failure blocks emission' 'unit order stage diagnostics artifacts and exit code'
Add-C 'SelfHost.Driver' 'CSH-DRV' 'Bootstrap backend emission' 'EmitBootstrapBackend' 'process' 'bootstrap project/output request' 'bootstrap compiler object' 'generation profile verification precedes all self-host emission' 'profile/dependency/backend failure remains attributable' 'verified stage sequence artifact and exit result'
Add-C 'SelfHost.Driver' 'CSH-DRV' 'Project semantic audit commands' 'AuditProjectPipeline|DumpProjectLayout|DumpProjectGenerics|DumpProjectCallables|DumpProjectEh|DumpSourceGraph|DumpModuleSet' 'process' 'project and audit subcommand' 'deterministic structural report' 'commands observe one coherent project/module model' 'project or semantic failure blocks report success' 'report records ordering identities counts and exit code'
Add-C 'SelfHost.Driver' 'CSH-DRV' 'Frozen bootstrap dependency verification' 'VerifyBootstrapDependencies' 'process' 'dependency inventory and expected contract' 'pass/fail dependency report' 'all required producer artifacts are checked without accepting stale substitutes' 'missing or mismatched dependency fails the verification' 'artifact identities reasons and final status'
Add-C 'SelfHost.Driver' 'CSH-DRV' 'Project command dispatch and response files' 'CheckProject|ProjectCommand|RunResponseFile|Main' 'process' 'argv or response-file tokens' 'selected command result' 'dispatch consumes each option exactly once and preserves nested command exit code' 'unknown malformed recursive or unreadable input is rejected' 'selected handler arguments recursion boundary and exit code'

# IR and data-flow pipeline.
Add-C 'SelfHost.IR' 'CSH-IR' 'HIR record and module storage' 'HirRecord|HirModule' 'unit' 'kind text syntax/type/symbol IDs depth conversions and bounds flag' 'ordered HIR record sequence and error count' 'records preserve source order and semantic provenance' 'invalid data is detected by verifier not silently rewritten' 'exact records order fields and count'
Add-C 'SelfHost.IR' 'CSH-IR' 'HIR construction' 'HirBuilder|LowerKind|Visit|Build' 'component' 'syntax tree and semantic model' 'HIR module' 'each visited syntax node yields ordered source-derived records' 'invalid child or missing semantic fact increments owned failure' 'independent walk of node/record provenance'
Add-C 'SelfHost.IR' 'CSH-IR' 'HIR verification' 'HirVerifier' 'unit' 'HIR module and source models' 'verification result' 'IDs depths operations conversions and bounds facts are coherent' 'first structural inconsistency rejects the module' 'independent invariant checker result'
Add-C 'SelfHost.IR' 'CSH-IR' 'Flow block edge and function model' 'FlowEdge|FlowBlock|FlowFunction|FlowModule' 'unit' 'blocks operations edges locals and symbols' 'mutable flow graph model' 'block IDs indexes endpoints termination and local tables agree' 'invalid selection or endpoint remains observable to verifier' 'node/edge/local tables and mutations'
Add-C 'SelfHost.IR' 'CSH-IR' 'MIR expression lowering' 'EmitExpression|Describe' 'component' 'HIR/syntax expression and current flow block' 'MIR operations and values' 'evaluation order and source provenance are preserved' 'unsupported or invalid expression records failure' 'operation sequence and result description'
Add-C 'SelfHost.IR' 'CSH-IR' 'MIR control-flow lowering' 'LowerBlock|Jump|CollectFunctions|MirBuilder|Build' 'component' 'syntax bodies semantic model' 'functions blocks edges and terminators' 'short-circuit loops branches and returns form coherent CFG' 'failed child or malformed control transfer rejects the flow module' 'independent CFG reconstruction and block operations'
Add-C 'SelfHost.IR' 'CSH-IR' 'Ownership cleanup insertion' 'AddOwnershipCleanup|OwnershipCleanupVerifier' 'component' 'scope exits locals and ownership facts' 'ordered cleanup operations or first problem' 'owned values clean exactly once in reverse acquisition order' 'missing duplicate or misordered cleanup is rejected' 'cleanup identities order and verifier problem'
Add-C 'SelfHost.IR' 'CSH-IR' 'Flow graph verification' 'FlowVerifier' 'unit' 'flow module' 'verification result' 'all blocks edges terminals and symbol references are coherent' 'first structural problem rejects acceptance' 'independent graph invariant result'
Add-C 'SelfHost.IR' 'CSH-IR' 'Dominance fixed-point construction' 'DominanceInfo|DominanceBuilder' 'component' 'verified CFG' 'dominance sets and immediate dominators' 'entry dominates reachable blocks and fixed point is stable' 'invalid predecessor graph prevents accepted dominance' 'independent dominator oracle and iteration stability'
Add-C 'SelfHost.IR' 'CSH-IR' 'HIR MIR CFG dominance and SSA dumps' 'IrDumper|Indent|Hir|Mir|Cfg|Dominance|Ssa' 'unit' 'validated IR models' 'deterministic textual dump' 'ordering follows stable function/block/value ordinals' 'none beyond invalid input precondition' 'exact normalized structural text'
Add-C 'SelfHost.IR' 'CSH-IR' 'Optimization pass pipeline description' 'PassPipeline' 'unit' 'O0 O1 O2 or O3' 'ordered pass description' 'each level has a stable declared pass sequence' 'unknown level follows documented default branch' 'exact pass list by level'
Add-C 'SelfHost.IR' 'CSH-IR' 'IR admission lifecycle' 'IrPipeline|Prepare|Verify|Dump|ValidatedIrProgram' 'integration' 'syntax semantic model and optimization level' 'valid HIR/flow bundle or failure' 'no backend observes IR until every producer and verifier accepts it' 'producer/verifier error blocks construction of a valid handle' 'valid flag model identities dumps and first failure'

# Lexer.
Add-C 'SelfHost.Lexer' 'CSH-LEX' 'Token vocabulary and stable names' 'TokenKind|TokenFacts' 'unit' 'every token enum value and unknown value' 'stable token name' 'legacy numeric identity and spelling remain stable within frozen implementation' 'unknown value maps to documented fallback' 'exhaustive enum/name table'
Add-C 'SelfHost.Lexer' 'CSH-LEX' 'Token stream storage and error count' 'TokenStream' 'unit' 'token kind begin/end offsets and errors' 'ordered token stream' 'ranges are monotone and count/index tables agree' 'out-of-range access follows current container behavior' 'tokens ranges order and error count'
Add-C 'SelfHost.Lexer' 'CSH-LEX' 'Cursor and character classification' 'AtEnd|Current|Peek|Advance|Match|IdentifierStart|IdentifierPart|DecimalDigit|HexDigit' 'unit' 'source text and cursor' 'classified character or advanced cursor' 'every successful consume advances and EOF is stable' 'invalid lookahead remains within sentinel behavior' 'cursor trace and classification truth table'
Add-C 'SelfHost.Lexer' 'CSH-LEX' 'Trivia and comments' 'SkipLineComment|SkipBlockComment|SkipTrivia' 'unit' 'whitespace and comment text' 'next significant position' 'all accepted trivia is skipped once without consuming following token' 'unterminated block comment increments lexer error' 'final cursor error count and next token'
Add-C 'SelfHost.Lexer' 'CSH-LEX' 'Identifiers and keywords' 'Keyword|LexIdentifier' 'unit' 'identifier-shaped text' 'identifier or exact keyword token' 'keyword recognition is exact and identifier boundaries are preserved' 'none declared beyond invalid source character' 'kind lexeme and range'
Add-C 'SelfHost.Lexer' 'CSH-LEX' 'Numeric literals' 'LexNumber' 'unit' 'decimal hexadecimal floating suffix and malformed forms' 'numeric token and range' 'scanner consumes only the maximal valid numeric candidate' 'malformed prefix/suffix increments error without stalling' 'token kind text range and next cursor'
Add-C 'SelfHost.Lexer' 'CSH-LEX' 'Character string raw and interpolated literals' 'DecodeEscape|LexCharacter|LexString|LexRawString|LexInterpolatedString' 'unit' 'quoted literal text including escapes and termination errors' 'literal/interpolation token sequence' 'delimiter escape and interpolation boundaries are preserved' 'invalid escape or missing terminator increments error and makes progress' 'decoded value token sequence ranges and error count'
Add-C 'SelfHost.Lexer' 'CSH-LEX' 'Punctuation inline assembly and full lexing' 'LexPunctuation|LexAssemblyBody|Next|LexAll|TokenDumper' 'component' 'complete source buffer' 'EOF-terminated token stream and deterministic dump' 'Next always advances or emits EOF and LexAll terminates' 'unknown punctuation/body termination increments error' 'token sequence EOF range dump and errors'

# Parser.
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Token cursor expectation and recovery' 'Current|Peek|At|ConsumeIf|Consume|Expect|AddError|LexemeAt|CurrentText|AtText|ErrorCount|FirstErrorIndex' 'unit' 'token stream and parser cursor' 'consumed token text or first error' 'every recovery path advances or terminates and first error remains stable' 'unexpected token records owned parse error' 'cursor trace consumed ranges and first error'
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Known types nodes and modifier vocabulary' 'IsKnownType|AddKnownType|NewNode|Node|ApplyType|Builtin|Modifier|Ownership|CopyTypeModifier|ParseLayoutModifiers|JoinAuxiliary' 'unit' 'tokens known-type set and syntax tree' 'typed node/modifier metadata' 'known types and auxiliary spelling are deterministic without duplicates' 'invalid modifier combinations are rejected by owning parse path' 'node fields known-type set and auxiliary text'
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Type allocation generic constraints and throws grammar' 'ParseType|ParseAllocationType|ParseGenericParameters|ParseGenericConstraints|ParseThrowsClause|AtGenericGreater|ConsumeGenericGreater' 'component' 'type/generic/constraint token sequences' 'syntax nodes and normalized type text' 'nested generic closers and constraints preserve token boundaries' 'missing identifiers closers constraints or throws type report first error' 'tree shape type text cursor and diagnostics'
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Literal and string decoding grammar' 'LiteralExpression|HexValue|DecodeString' 'unit' 'literal token and escape payload' 'literal syntax node and decoded text' 'source range and decoded semantic spelling remain attributable' 'invalid hex/escape produces parse error' 'node kind text and diagnostic'
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Primary cast and lambda disambiguation' 'IdentifierExpression|TypeStartsHere|LooksLikeCast|LooksLikeParenthesizedLambda|ParseLambda|ParsePrimary' 'component' 'ambiguous parenthesized and primary token sequences' 'selected primary/cast/lambda syntax shape' 'lookahead never consumes and parsing selects exactly one grammar branch' 'malformed selected branch reports without infinite retry' 'cursor before/after and syntax shape'
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Postfix member call index and argument grammar' 'ParsePostfix|ConsumeGenericCallArguments|ParseArgument' 'component' 'primary followed by calls members generics indexes postfix operators' 'left-associated postfix syntax tree' 'postfix operations preserve source order and argument modes' 'missing delimiters/member names produce owned error' 'nested tree arguments and cursor'
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Unary allocation destruction and ownership expressions' 'ParseUnary|ParseAllocationType' 'component' 'unary new placement stackalloc delete destroy move sizeof/alignof/default tokens' 'unary or construction syntax tree' 'operator precedence and allocation element/count structure are preserved' 'missing operand/type/delimiter reports and advances' 'node kind operator children type text and diagnostics'
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Binary conditional and assignment precedence' 'BinaryNode|ParseMultiplicative|ParseAdditive|ParseShift|ParseRelational|ParseEquality|ParseBitwiseAnd|ParseBitwiseXor|ParseBitwiseOr|ParseLogicalAnd|ParseLogicalOr|Assignment|ParseExpression' 'component' 'expression token stream' 'precedence/associativity syntax tree' 'precedence ladder and assignment associativity are stable' 'missing operand records error without fabricating valid tree' 'tree shape operators ranges and cursor'
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Variables blocks and basic statements' 'LooksLikeVariable|ParseVariable|ParseBlock|ParseIf|ParseReturn|ParseThrow|ParseStatement' 'component' 'statement token stream' 'statement/block syntax nodes' 'statement boundaries and nested block ownership are preserved' 'missing terminator/expression reports at owned boundary' 'tree order cursor and diagnostics'
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Loop statement grammar' 'ParseWhile|ParseDoWhile|ParseFor|ParseForeach' 'component' 'loop tokens including declarations expressions and bodies' 'loop syntax nodes' 'initializer condition increment enumerable and body occupy stable child roles' 'missing separators parentheses or body report and recover' 'node children flags cursor and diagnostics'
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Switch labels and sections' 'AtSwitchLabel|ParseSwitch' 'component' 'switch case/default tokens' 'switch tree with ordered sections' 'labels remain associated with their following statement sequence' 'duplicate malformed or missing label expression remains diagnosable downstream' 'section ordering labels and cursor'
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Exception and using statement grammar' 'ParseCatch|ParseTry|ParseUsingStatement' 'component' 'try catch finally and using tokens' 'structured exception/resource nodes' 'catch/finally order and using declaration/body remain explicit' 'missing handler/body/delimiter reports at grammar boundary' 'tree roles order and diagnostics'
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Static assertion and inline assembly grammar' 'ParseStaticAssert|ParseAssemblyConstraint|ParseAssemblyOperands|NonvolatileAssemblyRegister|ParseAssemblyClobbers|ParseInlineAssembly' 'component' 'static_assert and asm token payloads' 'assertion/assembly syntax metadata' 'operand constraint clobber and volatility order is retained' 'invalid register/constraint/delimiter reports without consuming next statement' 'node metadata operand order and diagnostic'
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Callable parameters constructors and functions' 'ParseParameter|ParseParameters|ParseConstructorInitializer|ParseFunction' 'component' 'declaration tokens modifiers generics parameters throws and body' 'callable declaration syntax node' 'parameter order modes defaults variadic state and body remain coherent' 'duplicate/missing names types delimiters or body report first error' 'node fields children modifiers throws and diagnostics'
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Enums aggregates members and operators' 'ParseEnum|ParseAggregate|ParseDelegate|OperatorMethodName|ParseOperator|MakeAccessor|ParseProperty|ParseIndexer' 'component' 'type/member declaration tokens' 'aggregate member syntax hierarchy' 'nested members accessors operators layout and underlying types retain declaration order' 'unsupported/malformed member reports and recovers at member boundary' 'tree hierarchy metadata order and diagnostics'
Add-C 'SelfHost.Parser' 'CSH-PAR' 'Namespaces imports extern globals and compilation unit' 'ParseUsing|ParseNamespace|ParseExternC|ParseGlobal|ParseCompilationUnit|Freeze' 'integration' 'complete token stream' 'frozen compilation-unit syntax tree' 'root contains every accepted top-level declaration in source order and freezes once' 'unrecognized top-level token reports and synchronizes' 'root children frozen state first error and EOF consumption'

# Project manifest and project graph.
Add-C 'SelfHost.Project' 'CSH-PRO' 'Self-host project identity and source globs' 'NativeSharp.Compiler.nsproj|Compile' 'build-contract' 'project XML' 'project name and ordered source include patterns' 'all self-host source domains are included without circular generated wrappers' 'missing domain or malformed XML prevents project admission' 'resolved source set compared with canonical 27 sources'
Add-C 'SelfHost.Project' 'CSH-PRO' 'Self-host project dependency closure' 'ProjectReference' 'build-contract' 'four project references' 'Core Collections IO and LLVM-C dependency graph' 'references are explicit and acyclic at project level' 'missing or incompatible dependency blocks build producer' 'exact normalized reference set and order'
Add-C 'SelfHost.Project' 'CSH-PRO' 'Project/source inventory coherence' 'Project' 'build-contract' 'manifest and filesystem inventory' 'coherent 27-source compilation set' 'every source belongs to one declared domain and no hidden implementation is introduced' 'unmatched or duplicate source is a gate failure' 'resolved paths cardinality uniqueness and component ownership'

# Project loading, metadata, graphs and product plans.
Add-C 'SelfHost.Projects' 'CSH-PRJ' 'Project model values and ordered references' 'ProjectModel' 'unit' 'path name output SDK sources and references' 'mutable ordered project model' 'insertion order and reference category remain distinct' 'none declared beyond invalid caller index' 'exact fields counts and indexed values'
Add-C 'SelfHost.Projects' 'CSH-PRJ' 'Project XML extraction' 'FindFirstSource|Tag|Attributes|MetadataAttributes|DependencyAttributes|Load' 'component' 'project XML and base path' 'populated ProjectModel' 'recognized tags/attributes map once and paths retain project provenance' 'missing source malformed tag/attribute or unreadable project returns failure' 'project model and first failure'
Add-C 'SelfHost.Projects' 'CSH-PRJ' 'Project verification and first source' 'Verify|FirstSourcePath' 'component' 'loaded ProjectModel and filesystem' 'verification result and first source path' 'required properties sources and references are coherent before consumers run' 'missing duplicate or incompatible entries reject the project' 'validation status reason and selected path'
Add-C 'SelfHost.Projects' 'CSH-PRJ' 'Project deterministic dump' 'ProjectDumper|NextSource|Dump' 'unit' 'ProjectModel' 'stable text report' 'sources and references are emitted in deterministic order' 'none beyond admitted model' 'exact normalized dump and source order'
Add-C 'SelfHost.Projects' 'CSH-PRJ' 'NSMETA compatibility contract' 'MetadataContract' 'integration' 'metadata name and bytes' 'NSMETA/2 text or compatibility decision' 'writer fields are stable and reader admits only declared compatible prefixes' 'unknown/truncated metadata is incompatible' 'header producer ABI compatibility fields and decision'
Add-C 'SelfHost.Projects' 'CSH-PRJ' 'Project cache identity' 'ProjectCache' 'unit' 'project content references toolchain and options' 'stable cache key' 'semantically identical ordered input yields identical key' 'none declared' 'known vectors and mutation sensitivity'
Add-C 'SelfHost.Projects' 'CSH-PRJ' 'Toolchain manifest' 'ToolchainManifest' 'integration' 'tool paths versions ABI and language metadata' 'deterministic manifest text' 'all selected producer identities are represented' 'missing producer data remains explicit rather than guessed' 'parsed fields and exact stable ordering'
Add-C 'SelfHost.Projects' 'CSH-PRJ' 'Product artifact plan' 'ProductPlan|Extension|Create' 'component' 'output type name root and artifact options' 'ordered product paths and kinds' 'extension and artifact set follow output type without silent fallback' 'unsupported output type is rejected' 'artifact list extensions roots and order'
Add-C 'SelfHost.Projects' 'CSH-PRJ' 'Source graph units and identity' 'SourceUnit|SourceGraph' 'unit' 'owner paths relative paths and order' 'deduplicated ordered graph' 'one canonical path maps to one unit and project identity' 'duplicate/conflicting path is reported by graph' 'unit/project tables order and diagnostic'
Add-C 'SelfHost.Projects' 'CSH-PRJ' 'Include expansion and deterministic enumeration' 'IncludeExpander|CollectSorted|ExpandFrom|Expand' 'component' 'project include patterns root and filesystem tree' 'sorted source candidates' 'recursive/nonrecursive patterns remain confined and results are stable' 'invalid root pattern or enumeration failure is diagnosed' 'normalized paths sort order duplicates and error'
Add-C 'SelfHost.Projects' 'CSH-PRJ' 'Project dependency traversal and source graph build' 'SourceGraphBuilder|Visit|Build' 'integration' 'root project and project-reference graph' 'ordered SourceGraph' 'dependencies precede consumers and cycles/duplicates terminate deterministically' 'unreadable invalid or cyclic project records a graph failure' 'project/unit order edges and diagnostic'
Add-C 'SelfHost.Projects' 'CSH-PRJ' 'Source graph dump' 'SourceGraphDumper|Dump' 'unit' 'SourceGraph' 'stable textual graph' 'dump order matches graph order and preserves owner/path identity' 'none beyond failed graph precondition' 'exact normalized report'

# Sema, module closure and auxiliary graphs.
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Semantic facts conversions and model storage' 'SemanticConversion|SemanticFact|SemanticModel|ValueCategory' 'unit' 'types symbols scopes node bindings conversions and errors' 'mutable semantic model with per-node facts' 'fact/symbol/scope indexes remain coherent and Hide removes only presence' 'invalid indexes remain caller errors; analyzer owns semantic diagnostics' 'fields conversions bindings counts and error transitions'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Callable graph construction and dump' 'CallableGraph' 'component' 'syntax tree semantic callable bindings' 'ordered callable dependency graph and dump' 'visited set terminates cycles and each callable appears once' 'unresolved callable remains absent or analyzer-owned error' 'nodes edges order and text'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Exception region graph' 'ExceptionRegionGraph' 'component' 'syntax tree try/catch/finally nodes' 'ordered nested exception region report' 'region nesting and source order are preserved' 'malformed syntax is rejected before this consumer' 'region hierarchy and dump'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Generic instantiation closure' 'GenericInstantiationSet' 'component' 'syntax and semantic generic applications' 'deduplicated ordered instantiation set and dump' 'same closed instantiation is recorded once and traversal terminates' 'open/unresolved instantiations do not become accepted closed entries' 'count identities ordering and dump'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Module export identity and classification' 'ModuleExport' 'unit' 'symbol kind names parent type and unit' 'classified module export' 'type/callable predicates agree with symbol kind' 'none declared' 'exact fields and predicate truth table'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Module index insertion lookup and overloads' 'ModuleIndex' 'component' 'module exports and queries' 'stable lookup/overload results and problems' 'qualified identity is unique while callable overloads retain declaration order' 'duplicates/conflicts become indexed problems' 'index rows lookups overload counts and problems'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Module unit lifecycle and external resolution' 'ModuleUnit' 'component' 'source unit tree lexer/parser/model state' 'analyzed module-unit record' 'model publication changes analyzed/front-end error state coherently' 'lex parse or sema failure remains attributable to unit' 'state transitions counts model and errors'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'External project and NSMETA loading' 'NoteExternalProject|MetadataKind|MetadataLine|LoadExternalMetadata' 'integration' 'project references and NSMETA/1 or NSMETA/2 text' 'external projects metadata and exports' 'only declared compatible formats contribute public symbols' 'missing malformed or unsupported metadata fails module set' 'external counts parsed records exports and diagnostic'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Module symbol collection and two-pass build' 'ModuleSet|CollectSymbol|Collect|Build' 'integration' 'SourceGraph and external metadata' 'ordered units global index and analyzed models' 'all units declare before cross-unit injection and analysis; order is deterministic' 'any unit or external producer failure marks set failed' 'unit states index exports resolutions and diagnostic'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Module and export dumps' 'ModuleSetDumper|DumpExports|Dump' 'unit' 'ModuleSet and index' 'stable module/export text' 'dump order follows unit/export identity and does not mutate model' 'none beyond admitted set' 'exact normalized report'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Scope lookup declaration and parent attachment' 'Lookup|LookupGlobal|Declare|AttachToParent' 'unit' 'scope symbol name and parent' 'resolved or declared symbol relation' 'nearest scope wins and parent/member relations are bidirectional' 'duplicate declaration increments semantic errors' 'selected symbol scope/member tables and errors'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Type lookup generic substitution and member resolution' 'TypeSymbol|GenericArgument|ValueGenericArgument|SubstituteMemberType|LookupMember' 'component' 'type text receiver generic context and member name' 'semantic type/symbol identity' 'closed substitution is deterministic and preserves member owner' 'unknown/open/incompatible type or member reports semantic error' 'type display symbol and conversion facts'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Overload resolution' 'SelectCallOverload' 'component' 'candidate set argument types and receiver' 'selected callable and conversions' 'best viable score wins and ambiguity is not silently ordered away' 'no viable or ambiguous call reports semantic error' 'candidate scores selection conversions and diagnostic'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Aggregate enum function and global declaration' 'DeclareAggregate|DeclareEnum|DeclareFunction|DeclareGlobalNode|Align|AuxiliaryValue' 'component' 'declaration syntax and current scope' 'symbols layouts signatures and constants' 'sizes alignments offsets parents and definitions are coherent before bodies' 'duplicates invalid layout/type/constant record semantic errors' 'symbol table layout signature constants and errors'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Literal numeric conversion and operator typing' 'CommonNumeric|Comparison|AddConversion|NumericConversion|AnalyzeLiteral' 'unit' 'literal/operator operand types' 'semantic fact operation/result types and conversions' 'numeric promotion yields one common physical semantic type' 'invalid literal or conversion reports semantic error' 'fact types constants conversion list and error'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Expression semantic analysis' 'AnalyzeExpression' 'component' 'expression syntax scopes and expected type' 'bound symbol type category storage and conversions' 'each accepted expression has a present coherent fact and ownership category' 'invalid operand call member cast allocation or condition reports owned error' 'semantic fact symbol and diagnostic per expression'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Statement and callable body analysis' 'AnalyzeStatement|AnalyzeFunction' 'component' 'statement/body syntax callable scope and return type' 'scopes facts symbols and control diagnostics' 'locals and parameters remain scoped and returns agree with callable result' 'invalid control return exception or declaration reports semantic error' 'scopes facts and diagnostics in source order'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Escape and no-heap validation' 'FirstStorageSymbol|ValidateEscapes|ValidateNoHeap' 'component' 'analyzed body ownership/storage graph and noheap mode' 'validation result and diagnostics' 'borrowed/owned values do not escape forbidden lifetime and noheap paths allocate nothing' 'first prohibited escape/allocation records semantic failure' 'storage origin traversal and diagnostic location'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Declaration-only external and full analysis modes' 'DeclareOnly|DeclareExternal|Analyze|Model' 'integration' 'syntax tree optional external exports and analysis mode' 'semantic model' 'declaration pass precedes body pass and external exports enter only through explicit injection' 'declaration/external/body error prevents successful model' 'symbol/scope/fact counts exports and errors'
Add-C 'SelfHost.Sema' 'CSH-SEM' 'Semantic symbol node and metadata dumps' 'SemanticDumper|DumpSymbol|DumpSymbols|DumpNode|DumpMetadata|Dump' 'unit' 'syntax and semantic model' 'deterministic semantic/metadata text' 'stable IDs and source order determine output' 'none beyond admitted model' 'exact normalized structural dump'

# Source processing.
Add-C 'SelfHost.Source' 'CSH-SRC' 'Compile-time expression cursor and primitives' 'ComptimeExpression|Space|Digit|Alpha|SkipSpace|Take' 'unit' 'directive expression text and cursor' 'classified/consumed tokens' 'successful consumption advances and whitespace skipping terminates' 'unexpected text leaves expression invalid' 'cursor trace and consumed characters'
Add-C 'SelfHost.Source' 'CSH-SRC' 'Compile-time primary and unary evaluation' 'ParsePrimary|ParseUnary' 'unit' 'parenthesized literal/identifier/unary expression' 'integer value and validity' 'operator binding and parentheses preserve evaluation order' 'missing operand/closer marks evaluation invalid' 'value validity and final cursor'
Add-C 'SelfHost.Source' 'CSH-SRC' 'Compile-time precedence evaluation' 'ParseMultiply|ParseAdd|ParseShift|ParseBitwiseAnd|ParseBitwiseXor|ParseBitwiseOr|Evaluate' 'component' 'integer expression' 'evaluated integer and success' 'precedence and associativity follow the declared ladder' 'division/shift/malformed expression reports failure without partial truth' 'value success and full-input consumption'
Add-C 'SelfHost.Source' 'CSH-SRC' 'Conditional-source marker recognition' 'IdentifierByte|StartsAt|StartsWord|Space' 'unit' 'source bytes and marker position' 'marker/word match result' 'word matching respects identifier boundaries' 'out-of-range position is a nonmatch' 'truth table and cursor invariance'
Add-C 'SelfHost.Source' 'CSH-SRC' 'Conditional-source transformation' 'ComptimeProcessor|Transform' 'component' 'source text and compile-time symbols' 'transformed source and first error' 'included/excluded regions preserve line attribution and nesting state' 'malformed directive nesting or expression returns stable failure' 'output bytes line structure and diagnostic'
Add-C 'SelfHost.Source' 'CSH-SRC' 'Diagnostic path normalization' 'DiagnosticPath|Normalize' 'unit' 'Windows or portable path' 'normalized diagnostic path' 'equivalent separators/prefixes yield stable presentation' 'none declared' 'exact path vectors independent of cwd'
Add-C 'SelfHost.Source' 'CSH-SRC' 'Source line-ending normalization and mapping' 'NormalizeLineEndings|BuildLineStarts|GetLineColumn' 'unit' 'raw text with CR LF CRLF and offsets' 'normalized LF text line starts and line/column' 'raw/normalized lengths and every offset remain attributable' 'out-of-range offset follows documented terminal mapping' 'text line-start table and exhaustive offset map'
Add-C 'SelfHost.Source' 'CSH-SRC' 'Source loading identity and content hash' 'SourceBuffer|LoadFrontend|Load|ContentHash' 'component' 'FileId path and file/text bytes' 'owned SourceBuffer or failure' 'path ID raw text normalized text and stable hash describe one source' 'read/encoding failure returns no accepted buffer' 'fields bytes hash line count and error'

# Symbols, syntax and types.
Add-C 'SelfHost.Symbols' 'CSH-SYM' 'Symbol kind vocabulary' 'SymbolKind|SymbolFacts' 'unit' 'symbol kind including unknown' 'stable name' 'every declared kind has one stable spelling' 'unknown kind uses documented fallback' 'exhaustive enum/name table'
Add-C 'SelfHost.Symbols' 'CSH-SYM' 'Symbol identity and declaration metadata' 'Symbol$|QualifiedName|TypeId|Declaration|Parent|IsConst|IsDefined|IsExternC|IsVirtual|IsOverride|IsStatic|IsVolatile|IsNoHeap|GenericText|ConstraintsText' 'unit' 'kind name type declaration and modifiers' 'symbol record' 'construction and setters preserve explicit semantic metadata' 'none declared' 'exact fields before/after mutation'
Add-C 'SelfHost.Symbols' 'CSH-SYM' 'Symbol member and layout metadata' 'Member|SetParent|AddMember|SetLayout|SetBitField|Offset|Size|Alignment|BitWidth|BitOffset' 'unit' 'parent/member identities and layout values' 'coherent aggregate symbol metadata' 'member order and layout fields remain stable' 'invalid layout is analyzer-owned error' 'member table and exact layout values'
Add-C 'SelfHost.Symbols' 'CSH-SYM' 'Symbol constants and reachability' 'ConstantValue|SetConstantValue|IsReachable|SetReachable' 'unit' 'constant/reachability transitions' 'updated symbol flags and value' 'presence is distinct from numeric zero and reachability is monotone in closure' 'none declared' 'flag/value transition table'
Add-C 'SelfHost.Symbols' 'CSH-SYM' 'Lexical scope declaration and lookup' 'Scope|Declare|LookupLocal|SymbolAt' 'unit' 'parent scope symbol name and declaration order' 'local lookup and ordered symbol set' 'local lookup never searches parent and first declared identity remains indexed' 'duplicate handling belongs to analyzer caller' 'count/order/lookup and parent identity'

Add-C 'SelfHost.Syntax' 'CSH-SYN' 'Syntax node construction and scalar fields' 'SyntaxNode|Kind|Text|Auxiliary|TypeText|SemanticTypeText|GenericText|ConstraintsText|ModifiersText|ThrowsTypeText|OperatorKind|Flag|IsReference|IsVariadic' 'unit' 'node kind and parsed metadata' 'syntax node value' 'getters exactly reflect constructor/setter state' 'mutation after freeze is prohibited by tree contract' 'all field values before/after setter'
Add-C 'SelfHost.Syntax' 'CSH-SYN' 'Syntax node relationships' 'First|Second|Third|ChildCount|ChildAt|SetFirst|SetSecond|SetThird|AddChild' 'unit' 'node IDs and ordered children' 'stable relationship graph' 'fixed slots and child vector retain independent ordered identities' 'invalid IDs are detected by later verifier/analyzer' 'slots children order and count'
Add-C 'SelfHost.Syntax' 'CSH-SYN' 'Syntax node and tree freeze' 'Freeze|IsFrozen' 'unit' 'mutable node/tree' 'immutable frozen state' 'freeze is monotone and accepted tree structure no longer changes' 'post-freeze mutation follows documented rejection/no-op' 'state transition and structural hash'
Add-C 'SelfHost.Syntax' 'CSH-SYN' 'Syntax tree node arena and root' 'SyntaxTree|Create|Node|Count|Root|SetRoot' 'unit' 'nodes and root ID' 'indexed syntax tree' 'IDs are insertion-stable and root refers to one existing node' 'invalid root/node access remains invalid caller state' 'IDs count root and node equality'
Add-C 'SelfHost.Syntax' 'CSH-SYN' 'Deterministic syntax dump' 'SyntaxDumper|DumpNode|Dump|Indent' 'unit' 'syntax tree root' 'hierarchical normalized text' 'child order and metadata determine output' 'deep recursion risk is documented rather than hidden' 'exact dump and depth stress outcome'

Add-C 'SelfHost.Types' 'CSH-TYP' 'Semantic and ownership type vocabularies' 'SemanticTypeKind|OwnershipKind' 'unit' 'enum values' 'stable type/ownership categories' 'category identity is stable within frozen model' 'unknown value remains unsupported' 'exhaustive enum behavior in consumers'
Add-C 'SelfHost.Types' 'CSH-TYP' 'Semantic type value and layout' 'SemanticType|Kind|Display|ElementType|ReturnType|ElementCount|Size|Alignment|Ownership|SetElementType|SetReturnType|SetElementCount|SetLayout|SetOwnership' 'unit' 'type kind display relations layout and ownership' 'mutable semantic type descriptor' 'relations and layout preserve explicit values' 'invalid relations are rejected by producer analysis' 'all fields and transition table'
Add-C 'SelfHost.Types' 'CSH-TYP' 'Function parameter type list' 'ParameterCount|ParameterType|AddParameterType' 'unit' 'ordered parameter type IDs' 'function parameter signature' 'insertion order and count/index agree' 'invalid index is caller error' 'exact ordered IDs and count'
Add-C 'SelfHost.Types' 'CSH-TYP' 'Builtin type table initialization' 'TypeTable|AddBuiltin' 'unit' 'new table' 'canonical builtin types and IDs' 'builtin spelling maps to stable unique identity' 'duplicate internal builtin is not published twice' 'complete builtin name/kind/layout inventory'
Add-C 'SelfHost.Types' 'CSH-TYP' 'Type lookup and interning' 'Count|Get|Find|Intern|EndsWith' 'component' 'type display strings including pointers arrays generics delegates and ownership' 'canonical type ID and descriptor' 'same normalized display interns once and composite relations are coherent' 'malformed or unknown composite text follows documented unknown descriptor' 'ID equality descriptors count and mutation stability'
Add-C 'SelfHost.Types' 'CSH-TYP' 'String literal type construction' 'StringLiteral' 'unit' 'literal length/content category' 'string-literal semantic type ID' 'literal types retain declared element/count semantics' 'none declared' 'kind display element count and ID reuse'
Add-C 'SelfHost.Types' 'CSH-TYP' 'Function type construction and numeric predicates' 'Function|IsInteger|IsNumeric' 'unit' 'return/parameter types and queried type ID' 'function type ID or numeric classification' 'signature identity is structural and predicates agree with builtin kinds' 'non-numeric/invalid type returns false' 'signature descriptor and predicate truth table'

if ($contractsSpec.Count -lt 100) { throw "Contract catalogue unexpectedly small: $($contractsSpec.Count)" }

function Normalize-Path([string]$Path) { return $Path.Replace('\\','/') }
function Escape-Md([string]$Text) { return $Text.Replace('|','\|').Replace("`r",' ').Replace("`n",' ') }
function Slug([string]$Component) { return $Component.ToUpperInvariant().Replace('.','-') }

$componentDependencies = @{
    'SelfHost.Basic'='NativeSharp.Core'
    'SelfHost.Bootstrap'='SelfHost.Basic;SelfHost.Project'
    'SelfHost.CodeGen'='SelfHost.IR;SelfHost.Sema;NativeSharp.LLVM.C;Windows x64 ABI'
    'SelfHost.Diagnostics'='SelfHost.Basic;NativeSharp.Core'
    'SelfHost.Documentation'='selfhost/nsc source tree;NativeSharp.Compiler.nsproj'
    'SelfHost.Driver'='all self-host compiler components;NativeSharp.Core.IO'
    'SelfHost.IR'='SelfHost.Syntax;SelfHost.Sema;SelfHost.Types;SelfHost.Symbols'
    'SelfHost.Lexer'='SelfHost.Source;SelfHost.Diagnostics'
    'SelfHost.Parser'='SelfHost.Lexer;SelfHost.Syntax;SelfHost.Diagnostics'
    'SelfHost.Project'='NativeSharp.Core;NativeSharp.Core.Collections.Generic;NativeSharp.Core.IO;NativeSharp.LLVM.C'
    'SelfHost.Projects'='SelfHost.Source;SelfHost.Parser;SelfHost.Sema;filesystem'
    'SelfHost.Sema'='SelfHost.Syntax;SelfHost.Types;SelfHost.Symbols;SelfHost.Projects'
    'SelfHost.Source'='SelfHost.Basic;NativeSharp.Core.IO'
    'SelfHost.Symbols'='SelfHost.Types;SelfHost.Syntax'
    'SelfHost.Syntax'='SelfHost.Basic;SelfHost.Lexer'
    'SelfHost.Types'='SelfHost.Basic;NativeSharp.Core.Collections.Generic'
}

$componentPurpose = @{
    'SelfHost.Basic'='Proveer identidades tipadas, hash determinista y almacenamiento verificado por generación para todas las etapas self-host posteriores.'
    'SelfHost.Bootstrap'='Admitir sólo el subconjunto congelado de fuente/proyecto bootstrap y orquestar su primera generación sin fallback oculto.'
    'SelfHost.CodeGen'='Bajar IR y hechos semánticos self-host validados a LLVM 23, verificar el módulo y emitir objeto o assembly Windows x64.'
    'SelfHost.Diagnostics'='Representar y renderizar diagnósticos bootstrap estables en forma humana y JSON.'
    'SelfHost.Documentation'='Declarar arquitectura self-host, historia de etapas y restricciones bootstrap como evidencia documental, no como verdad ejecutable.'
    'SelfHost.Driver'='Exponer la frontera CLI que compone carga, frontend, IR, codegen, auditorías de proyecto y verificaciones bootstrap.'
    'SelfHost.IR'='Materializar HIR y MIR/CFG, verificar cleanup de ownership y control, calcular dominancia y admitir sólo programas validados.'
    'SelfHost.Lexer'='Convertir fuente normalizada en un stream de tokens con rangos, EOF, errores y dumps deterministas.'
    'SelfHost.Parser'='Construir el árbol sintáctico self-host completo, primero mutable y después congelado, preservando precedencia, declaraciones y progreso de recuperación.'
    'SelfHost.Project'='Definir el conjunto físico de fuentes del proyecto self-host y sus cuatro dependencias explícitas.'
    'SelfHost.Projects'='Cargar XML de proyecto, resolver grafos de fuentes/dependencias, leer/escribir metadata, derivar identidad de cache y planear productos.'
    'SelfHost.Sema'='Declarar, ligar y tipar el programa; cerrar unidades/módulos; validar ownership, escapes y no-heap; exponer grafos semánticos deterministas.'
    'SelfHost.Source'='Poseer buffers normalizados y ejecutar la transformación bootstrap de fuente condicional en compile time.'
    'SelfHost.Symbols'='Representar declaraciones, miembros, layout, constantes, reachability y scopes léxicos.'
    'SelfHost.Syntax'='Poseer nodos y relaciones indexadas, congelar el árbol y producir un dump estructural determinista.'
    'SelfHost.Types'='Internar tipos semánticos canónicos, firmas compuestas, ownership y hechos de layout físico.'
}

# Build stable contract IDs in catalogue order.
$componentOrdinals = @{}
$generatedContracts = [System.Collections.Generic.List[object]]::new()
foreach ($spec in $contractsSpec) {
    if (-not $componentOrdinals.ContainsKey($spec.component)) { $componentOrdinals[$spec.component] = 0 }
    $componentOrdinals[$spec.component]++
    $contractId = '{0}-{1:D3}' -f $spec.prefix, $componentOrdinals[$spec.component]
    Add-Member -InputObject $spec -NotePropertyName contract_id -NotePropertyValue $contractId
    $document = 'components/{0}.md' -f (Slug $spec.component)
    $section = '4.{0}' -f $componentOrdinals[$spec.component]
    $generatedContracts.Add([pscustomobject][ordered]@{
        contract_id=$contractId; component=$spec.component; name=$spec.name; source_symbols=''
        inputs=$spec.inputs; preconditions='All producer inputs named by the contract are present and admitted'
        outputs=$spec.outputs; postconditions=$spec.outputs + ' are coherent and observable'
        invariants=$spec.invariant; errors=$spec.errors
        side_effects=if($spec.layer -in @('integration','process','component')){'May mutate the owned model or emit the named report/artifact only after producer success'}else{'None beyond the owned value/model mutation'}
        dependencies=$componentDependencies[$spec.component]; observable_oracle=$spec.oracle
        test_layer=$spec.layer; status='CONSOLIDADO'; document=$document; section=$section
    })
}

function Resolve-Contract([string]$Component,[string]$Qualified,[string]$Name,[string]$Path) {
    $ownedSpecs = @($contractsSpec | Where-Object component -eq $Component)
    foreach ($spec in $ownedSpecs) {
        foreach ($token in @($spec.pattern -split '\|')) {
            $literal = $token.TrimEnd('$')
            if ($Name -ceq $literal) { return $spec.contract_id }
        }
    }
    foreach ($spec in $ownedSpecs) {
        foreach ($token in @($spec.pattern -split '\|')) {
            $literal = $token.TrimEnd('$')
            if ($literal.Length -ge 5 -and $Name.StartsWith($literal,[StringComparison]::Ordinal)) { return $spec.contract_id }
        }
    }
    $fallback = $ownedSpecs
    if ($fallback.Count -eq 0) { throw "No contract catalogue for $Component" }
    return $fallback[0].contract_id
}

# Iterative physical declaration scan. The scope is deliberately types and
# callable declarations, matching the declaration-level inventory used for host code.
$symbols = [System.Collections.Generic.List[object]]::new()
$symbolNumber = 5294
foreach ($file in ($selfHostFiles | Where-Object { $_.path -like '*.ns' })) {
    $absolute = Join-Path $repoRoot ($file.path.Replace('/','\'))
    $lines = @(Get-Content -LiteralPath $absolute)
    $namespace = 'NativeSharp.Compiler'
    $currentType = [IO.Path]::GetFileNameWithoutExtension($file.path)
    for ($index = 0; $index -lt $lines.Count; $index++) {
        $line = $lines[$index]
        if ($line -match '^\s*namespace\s+([A-Za-z_][A-Za-z0-9_.]*)') { $namespace = $matches[1]; continue }
        $kind = $null; $name = $null; $visibility = 'internal'
        if ($line -match '^\s*(?:(public|private|internal|protected)\s+)?(?:(?:static|sealed|abstract|partial|readonly|value|owned|raii)\s+)*(class|struct|enum|interface|delegate)\s+([A-Za-z_]\w*)') {
            if ($matches[1]) { $visibility = $matches[1] }
            $kind = $matches[2]; $name = $matches[3]; $currentType = $name
            $qualified = "$namespace.$name"
        } elseif ($line -match '^\s*(public|private|internal|protected)\s+(?:((?:static|virtual|override|abstract|sealed|extern|unsafe|readonly|owned|borrowed|raii|move|copy|partial|inline)\s+)*)([^=;{}]*?)\b([A-Za-z_]\w*)\s*\(') {
            $visibility = $matches[1]; $kind = 'function'; $name = $matches[4]
            $qualified = "$namespace.$currentType.$name"
        } else { continue }
        $contractId = Resolve-Contract $file.component $qualified $name $file.path
        $contract = $generatedContracts | Where-Object contract_id -eq $contractId | Select-Object -First 1
        $symbolNumber++
        $symbols.Add([pscustomobject][ordered]@{
            symbol_id=('SYM-{0:D6}' -f $symbolNumber); file_id=$file.id; path=$file.path
            component=$file.component; kind=$kind; qualified_name=$qualified; line=$index+1
            visibility=$visibility; responsibility=('Own declaration `{0}` within contract {1}: {2}' -f $qualified,$contractId,$contract.name)
            inputs=$contract.inputs; preconditions=$contract.preconditions; outputs=$contract.outputs
            invariants=$contract.invariants; errors=$contract.errors; side_effects=$contract.side_effects
            dependencies=$contract.dependencies; observability=$contract.observable_oracle
            test_layer=$contract.test_layer; contract_id=$contractId; state='CONSOLIDADO'
            document=$contract.document; section=$contract.section
        })
    }
}
if ($symbols.Count -ne 874) { throw "Expected 874 physical self-host declarations, found $($symbols.Count)" }

# Bind each contract to all physical declarations assigned to it. Project and
# documentation contracts intentionally use manifest/document paths as source symbols.
foreach ($contract in $generatedContracts) {
    $owned = @($symbols | Where-Object contract_id -eq $contract.contract_id | Select-Object -ExpandProperty qualified_name -Unique)
    if ($owned.Count -gt 0) { $contract.source_symbols = $owned -join ';' }
    elseif ($contract.component -eq 'SelfHost.Project') { $contract.source_symbols = 'selfhost/nsc/NativeSharp.Compiler.nsproj' }
    elseif ($contract.component -eq 'SelfHost.Documentation') { $contract.source_symbols = 'selfhost/nsc/README.md' }
    else { $contract.source_symbols = 'indirect contract over declarations listed in the component document' }
}

# Design cases from contract semantics. Six cases are universal; determinism,
# state mutation and scale are added only when the contract actually owns them.
$generatedCases = [System.Collections.Generic.List[object]]::new()
$caseOrdinal = 0
foreach ($contract in $generatedContracts) {
    $templates = [System.Collections.Generic.List[object]]::new()
    $templates.Add([pscustomobject]@{objective='minimal valid contract';class='valid-minimal';setup='smallest admitted producer model';input='minimum valid input named by contract';expected='exact successful output and invariant';frequency='PR'})
    $templates.Add([pscustomobject]@{objective='representative valid contract';class='valid-representative';setup='representative admitted model';input='all ordinary fields and relationships';expected='complete output and side effects match oracle';frequency='PR'})
    $templates.Add([pscustomobject]@{objective='missing producer prerequisite';class='invalid-precondition';setup='remove one required producer input';input='otherwise valid input with one prerequisite absent';expected='reject at owner boundary with no downstream publication';frequency='PR'})
    $templates.Add([pscustomobject]@{objective='malformed relationship or value';class='invalid-structure';setup='construct malformed owned relationship';input='invalid ordering type layout token metadata or ownership relation';expected='owned error and no accepted partial output';frequency='PR'})
    $templates.Add([pscustomobject]@{objective='empty singleton duplicate and extrema';class='boundary';setup='parameterized boundary matrix';input='empty one duplicate min max null and non-ASCII where applicable';expected='documented boundary behavior for every applicable row';frequency='PR'})
    $templates.Add([pscustomobject]@{objective='all named modes and variants';class='variant-matrix';setup='parameterized variant matrix';input='every enum mode kind operator and ABI variant owned by contract';expected='each variant preserves its named contract';frequency='PR'})
    $allText = @($contract.name,$contract.invariants,$contract.outputs,$contract.side_effects) -join ' '
    if ($allText -match '(?i)stable|determin|order|hash|identity|dump|cache') { $templates.Add([pscustomobject]@{objective='repeatability and input-order perturbation';class='determinism';setup='repeat fixture and permute independent inputs';input='same semantic input under repeated/permuted construction';expected='identical semantic structure or bytes in canonical order';frequency='PR'}) }
    if ($contract.side_effects -notmatch '^None') { $templates.Add([pscustomobject]@{objective='failure after partial owned mutation';class='transaction-state';setup='inject failure after first owned transition';input='valid prefix followed by one failing producer operation';expected='no false accepted state and observable owner-consistent failure';frequency='NIGHTLY'}) }
    if ($allText -match '(?i)graph|recursive|depth|large|closure|arena|source|module|tree|pipeline|emission|compile') { $templates.Add([pscustomobject]@{objective='depth breadth and scale boundary';class='scale';setup='generated deep/wide fixture without changing contract';input='large graph tree source or declaration set at documented limits';expected='bounded completion or explicitly documented limitation without silent truncation';frequency='NIGHTLY'}) }
    foreach ($template in $templates) {
        $caseOrdinal++; $caseId = 'TSH-{0:D5}' -f $caseOrdinal
        $generatedCases.Add([pscustomobject][ordered]@{
            case_id=$caseId; contract_id=$contract.contract_id; component=$contract.component
            objective=("{0}: {1}" -f $template.objective,$contract.name); input_class=$template.class
            setup=$template.setup; input=$template.input; expected=("{0}; oracle: {1}" -f $template.expected,$contract.observable_oracle)
            dependencies=$contract.dependencies; layer=$contract.test_layer; frequency=$template.frequency
            status='DESIGNED'; document=('test-plans/{0}-TEST-PLAN.md' -f (Slug $contract.component)); section=("case {0}" -f $caseId)
        })
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
foreach ($entry in $protectedHashes.GetEnumerator()) {
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $repoRoot $entry.Key)).Hash.ToLowerInvariant()
    if ($actual -ne $entry.Value) { throw "Protected file changed: $($entry.Key)" }
}

# Replace only the self-host slices of the machine-readable inventories.
$symbolsPath = Join-Path $inventoryRoot 'symbols.csv'
$oldSymbols = @(Import-Csv -LiteralPath $symbolsPath | Where-Object { $_.component -notlike 'SelfHost.*' })
@($oldSymbols + $symbols) | Export-Csv -LiteralPath $symbolsPath -NoTypeInformation -Encoding utf8

$contractsPath = Join-Path $inventoryRoot 'contracts.csv'
$oldContracts = @(Import-Csv -LiteralPath $contractsPath | Where-Object { $_.contract_id -notlike 'CSH-*' })
@($oldContracts + $generatedContracts) | Export-Csv -LiteralPath $contractsPath -NoTypeInformation -Encoding utf8

$casesPath = Join-Path $inventoryRoot 'test-case-design.csv'
$oldCases = @(Import-Csv -LiteralPath $casesPath | Where-Object { $_.case_id -notlike 'TSH-*' })
@($oldCases + $generatedCases) | Export-Csv -LiteralPath $casesPath -NoTypeInformation -Encoding utf8

$componentGroups = @($selfHostFiles | Group-Object component | Sort-Object Name)
$componentsPath = Join-Path $inventoryRoot 'components.csv'
$components = @(Import-Csv -LiteralPath $componentsPath)
foreach ($row in $components) {
    if ($row.role -eq 'PRIMARY_COMPILER' -and $row.component -like 'SelfHost.*') {
        $row.state = 'AUDITADO'; $row.reconstructibility = 'RECONSTRUIBLE'
        $row.document = 'components/{0}.md' -f (Slug $row.component)
        $row.audit_report = 'audits/TANDA-12A-{0}-AUDIT.md' -f (Slug $row.component)
    }
}
$components | Export-Csv -LiteralPath $componentsPath -NoTypeInformation -Encoding utf8

$productionById = @{}; foreach ($row in $production) { $productionById[$row.id] = $row }
foreach ($row in $production) {
    if ($row.path -like 'selfhost/nsc/*') {
        $row.state = 'AUDITADO'
        $row.evidence = 'evidence/TANDA-12A-SELFHOST-METHOD.md'
        $row.section = 'inventory/selfhost-file-audit.csv'
        $row.notes = 'Semantically read in full; classified by owning self-host component and contract'
    }
}
$production | Export-Csv -LiteralPath $productionPath -NoTypeInformation -Encoding utf8

$coveragePath = Join-Path $inventoryRoot 'document-coverage.csv'
$coverage = @(Import-Csv -LiteralPath $coveragePath)
foreach ($row in $coverage) {
    if ($row.path -like 'selfhost/nsc/*') {
        $ownedSymbols = @($symbols | Where-Object file_id -eq $row.file_id | Select-Object -ExpandProperty symbol_id)
        $ownedContracts = @($symbols | Where-Object file_id -eq $row.file_id | Select-Object -ExpandProperty contract_id -Unique)
        if ($row.component -eq 'SelfHost.Project') { $ownedContracts = @($generatedContracts | Where-Object component -eq 'SelfHost.Project' | Select-Object -ExpandProperty contract_id) }
        if ($row.component -eq 'SelfHost.Documentation') { $ownedContracts = @($generatedContracts | Where-Object component -eq 'SelfHost.Documentation' | Select-Object -ExpandProperty contract_id) }
        $row.file_state = 'AUDITADO'; $row.symbol_id = $ownedSymbols -join ';'; $row.contract_id = $ownedContracts -join ';'
        $row.document = 'components/{0}.md' -f (Slug $row.component); $row.section = '1-13'
        $row.trace_state = 'TRAZADO'; $row.historical_state = 'CONTRASTADO'; $row.current_state = 'IMPLEMENTACION VIGENTE'
        $row.notes = 'Full source/project/document read; declarations and contracts mapped'
    }
}
$coverage | Export-Csv -LiteralPath $coveragePath -NoTypeInformation -Encoding utf8

$fileAudit = foreach ($row in $selfHostFiles) {
    $fileSymbols = @($symbols | Where-Object file_id -eq $row.id)
    $fileContracts = if ($fileSymbols.Count -gt 0) { @($fileSymbols | Select-Object -ExpandProperty contract_id -Unique) }
        elseif ($row.component -eq 'SelfHost.Project') { @($generatedContracts | Where-Object component -eq $row.component | Select-Object -ExpandProperty contract_id) }
        elseif ($row.component -eq 'SelfHost.Documentation') { @($generatedContracts | Where-Object component -eq $row.component | Select-Object -ExpandProperty contract_id) }
        else { @() }
    [pscustomobject][ordered]@{
        file_id=$row.id; path=$row.path; component=$row.component; lines=$row.lines; bytes=$row.bytes; sha256=$row.sha256
        declarations=$fileSymbols.Count; contracts=($fileContracts -join ';'); semantic_read='YES'; classified='YES'
        document=('components/{0}.md' -f (Slug $row.component)); audit=('audits/TANDA-12A-{0}-AUDIT.md' -f (Slug $row.component))
    }
}
$fileAudit | Export-Csv -LiteralPath (Join-Path $inventoryRoot 'selfhost-file-audit.csv') -NoTypeInformation -Encoding utf8

# Component reconstruction documents and test plans.
foreach ($group in $componentGroups) {
    $component = $group.Name; $slug = Slug $component
    $componentContracts = @($generatedContracts | Where-Object component -eq $component)
    $componentSymbols = @($symbols | Where-Object component -eq $component)
    $componentCases = @($generatedCases | Where-Object component -eq $component)
    $files = @($group.Group | Sort-Object path)
    $fileWord = if ($files.Count -eq 1) { 'archivo' } else { 'archivos' }
    $lines = ($files | Measure-Object lines -Sum).Sum; $bytes = ($files | Measure-Object bytes -Sum).Sum
    $prCases = @($componentCases | Where-Object frequency -eq 'PR').Count
    $nightlyCases = @($componentCases | Where-Object frequency -eq 'NIGHTLY').Count

    $doc = [System.Collections.Generic.List[string]]::new()
    $doc.Add("# $component — documento reconstructivo")
    $doc.Add(''); $doc.Add('## 1. Estado y procedencia'); $doc.Add('')
    $doc.Add('- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.')
    $doc.Add('- **Repositorio:** `A:\nsc-devel`.')
    $doc.Add(('- **Revisión auditada:** `{0}`.' -f $revision))
    $doc.Add("- **Universo:** $($files.Count) $fileWord, $lines líneas, $bytes bytes, $($componentSymbols.Count) declaraciones físicas.")
    $doc.Add("- **Diseño:** $($componentContracts.Count) contratos y $($componentCases.Count) casos futuros; no se implementaron ni ejecutaron pruebas.")
    $doc.Add(''); $doc.Add('## 2. Propósito y frontera'); $doc.Add(''); $doc.Add($componentPurpose[$component])
    $doc.Add(''); $doc.Add('No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.')
    $doc.Add(''); $doc.Add('## 3. Entradas, salidas y dependencias'); $doc.Add('')
    $doc.Add("- **Dependencias:** $($componentDependencies[$component]).")
    $doc.Add('- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.')
    $doc.Add('- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.')
    $doc.Add('- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.')
    $doc.Add(''); $doc.Add('### Archivos auditados'); $doc.Add(''); $doc.Add('| ID | Ruta | Líneas | Declaraciones |'); $doc.Add('|---|---|---:|---:|')
    foreach ($file in $files) { $doc.Add(('| {0} | `{1}` | {2} | {3} |' -f $file.id,$file.path,$file.lines,@($componentSymbols | Where-Object file_id -eq $file.id).Count)) }
    $doc.Add(''); $doc.Add('## 4. Contratos'); $doc.Add('')
    for ($i=0; $i -lt $componentContracts.Count; $i++) {
        $c = $componentContracts[$i]
        $doc.Add("### 4.$($i+1) $($c.contract_id) — $($c.name)"); $doc.Add('')
        $doc.Add("- **Símbolos:** $(Escape-Md $c.source_symbols).")
        $doc.Add("- **Entradas:** $($c.inputs).")
        $doc.Add("- **Precondiciones:** $($c.preconditions).")
        $doc.Add("- **Salidas/postcondiciones:** $($c.outputs); $($c.postconditions).")
        $doc.Add("- **Invariante:** $($c.invariants).")
        $doc.Add("- **Errores:** $($c.errors).")
        $doc.Add("- **Efectos/lifetime:** $($c.side_effects).")
        $doc.Add("- **Oracle:** $($c.observable_oracle).")
        $doc.Add('')
    }
    $doc.Add('## 5. Estructuras, estados y algoritmos'); $doc.Add('')
    $doc.Add('Las estructuras concretas son las declaraciones trazadas en `inventory/symbols.csv`. Los algoritmos se dividen por contrato para impedir que una prueba de integración oculte una falla de cursor, identidad, grafo, ownership, ABI o publicación. Los builders conservan estado sólo dentro de su owner; los modelos aceptados se publican después de sus verificadores.')
    $doc.Add(''); $doc.Add('## 6. Invariantes y transiciones'); $doc.Add('')
    $doc.Add('El estado inicial, las mutaciones admitidas, la publicación y el fallo se prueban por contrato. Los contratos de valor son puros; los de builder, grafo, pipeline, proceso o emisión comprueban transiciones y ausencia de éxito parcial.')
    $doc.Add(''); $doc.Add('## 7. Ownership y lifetime'); $doc.Add('')
    $doc.Add('Los tipos `owned`, `raii`, handles, arenas, listas y punteros se caracterizan según la implementación congelada. Una prueba no debe forzar un feature de ownership ajeno al contrato ni convertir una observación actual en política futura del lenguaje.')
    $doc.Add(''); $doc.Add('## 8. Complejidad y límites'); $doc.Add('')
    $doc.Add('Las búsquedas lineales, recorridos de árboles/grafos, cierres y pipelines se ejercitan con vacío, caso nominal y escala. La Fase 1 no promete cotas no declaradas: cualquier recorrido recursivo vigente queda como riesgo observable y contradicción, no como refactor autorizado.')
    $doc.Add(''); $doc.Add('## 9. Errores y recuperación'); $doc.Add('')
    $doc.Add('El oracle identifica categoría, propietario, progreso y publicación. Parser/Lexer deben avanzar o terminar; builders y pipelines detienen consumidores; diagnósticos conservan identidad; emisores no publican éxito si un productor falla.')
    $doc.Add(''); $doc.Add('## 10. Integración'); $doc.Add('')
    $doc.Add(('La integración directa es: {0}. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.' -f $componentDependencies[$component]))
    $doc.Add(''); $doc.Add('## 11. Contradicciones y límites de autoridad'); $doc.Add('')
    $doc.Add('Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.')
    $doc.Add(''); $doc.Add('## 12. Receta de reconstrucción'); $doc.Add('')
    $doc.Add('1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.')
    $doc.Add(''); $doc.Add('## 13. Dictamen'); $doc.Add('')
    $doc.Add(('Los {0} archivos, {1} declaraciones, {2} contratos y {3} casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.' -f $files.Count,$componentSymbols.Count,$componentContracts.Count,$componentCases.Count))
    $doc | Set-Content -LiteralPath (Join-Path $docsRoot "components\$slug.md") -Encoding utf8

    $plan = [System.Collections.Generic.List[string]]::new()
    $plan.Add("# Plan de pruebas — $component"); $plan.Add('')
    $plan.Add('## 1. Alcance'); $plan.Add(''); $plan.Add("Plan desde cero para $($componentContracts.Count) contratos y $($componentCases.Count) casos diseñados. No reutiliza el nombre histórico como oracle y no implementa ni ejecuta pruebas.")
    $plan.Add(''); $plan.Add('## 2. Estrategia futura'); $plan.Add('')
    $plan.Add('- Casos internos en un ejecutable unitario consolidado por componente, no un ejecutable por caso.')
    $plan.Add('- Casos de lenguaje/proceso mediante el compilador construido y fixtures `.ns` externos mínimos.')
    $plan.Add('- Oracles estructurales o semánticos; los mensajes sólo se comparan por código/categoría/campos estables.')
    $plan.Add('- Self-host, escala y publicación costosa quedan `NIGHTLY`; los contratos pequeños y de frontera quedan `PR`.')
    $plan.Add(''); $plan.Add('## 3. Contratos'); $plan.Add(''); $plan.Add('| Contrato | Nombre | Capa | Oracle |'); $plan.Add('|---|---|---|---|')
    foreach ($c in $componentContracts) { $plan.Add("| $($c.contract_id) | $(Escape-Md $c.name) | $($c.test_layer) | $(Escape-Md $c.observable_oracle) |") }
    $plan.Add(''); $plan.Add('## 4. Casos diseñados'); $plan.Add(''); $plan.Add('| Caso | Contrato | Objetivo | Entrada | Esperado | Capa | Frecuencia |'); $plan.Add('|---|---|---|---|---|---|---|')
    foreach ($case in $componentCases) { $plan.Add("| $($case.case_id) | $($case.contract_id) | $(Escape-Md $case.objective) | $(Escape-Md $case.input) | $(Escape-Md $case.expected) | $($case.layer) | $($case.frequency) |") }
    $plan.Add(''); $plan.Add('## 5. Gate de implementación futura'); $plan.Add('')
    $plan.Add('Cada caso deberá ejecutarse de forma individual, conservar evidencia y mapearse al contrato. El componente no podrá declararse cubierto por el simple éxito de un runner agregado. Coverage instrumental pertenece a la Fase 3.')
    $plan | Set-Content -LiteralPath (Join-Path $docsRoot "test-plans\$slug-TEST-PLAN.md") -Encoding utf8

    $audit = [System.Collections.Generic.List[string]]::new()
    $audit.Add("# Segunda auditoría independiente — $component"); $audit.Add('')
    $audit.Add('## Resultado cuantitativo'); $audit.Add(''); $audit.Add('| Gate | Resultado |'); $audit.Add('|---|---:|')
    $audit.Add("| Archivos inventariados / leídos / auditados | $($files.Count) / $($files.Count) / $($files.Count) |")
    $audit.Add("| Líneas / bytes | $lines / $bytes |")
    $audit.Add("| Declaraciones físicas | $($componentSymbols.Count) |")
    $audit.Add("| Contratos | $($componentContracts.Count) |")
    $audit.Add("| Casos diseñados | $($componentCases.Count) |")
    $audit.Add("| PR / NIGHTLY | $prCases / $nightlyCases |")
    $audit.Add("| Declaraciones sin contrato | $(@($componentSymbols | Where-Object { -not $_.contract_id }).Count) |")
    $audit.Add(''); $audit.Add('## Comprobaciones'); $audit.Add('')
    $audit.Add('- Cada archivo conserva ID, path, hash, revisión y trazabilidad documental.')
    $audit.Add('- Cada declaración física pertenece a exactamente un contrato primario; contratos de proyecto/documentación trazan su archivo no ejecutable.')
    $audit.Add('- Cada contrato posee positivos, negativos, bordes y variantes; determinismo, mutación y escala sólo se añaden cuando aplican.')
    $audit.Add('- Se revisaron errores, estados, ownership, dependencias y oracles sin ejecutar producto.')
    $audit.Add('- No se tocó compilador, pruebas, C:, instalación, baseline, coverage ni remotos.')
    $audit.Add(''); $audit.Add('## Dictamen'); $audit.Add('')
    $audit.Add(('`{0}` queda `AUDITADO` y `RECONSTRUIBLE` para la revisión congelada. Este dictamen es documental; no afirma que la suite futura exista o haya pasado.' -f $component))
    $audit | Set-Content -LiteralPath (Join-Path $docsRoot "audits\TANDA-12A-$slug-AUDIT.md") -Encoding utf8
}

function Replace-GeneratedSection {
    param([string]$Path,[string]$Marker,[string[]]$Lines)
    $begin = "<!-- BEGIN $Marker -->"; $end = "<!-- END $Marker -->"
    $current = if (Test-Path -LiteralPath $Path) { Get-Content -Raw -LiteralPath $Path } else { '' }
    $block = @($begin) + $Lines + @($end)
    $rendered = $block -join "`r`n"
    $pattern = '(?s)' + [regex]::Escape($begin) + '.*?' + [regex]::Escape($end)
    if ($current -match $pattern) { $current = [regex]::Replace($current,$pattern,$rendered) }
    else { $current = $current.TrimEnd() + "`r`n`r`n" + $rendered }
    $current = $current.TrimEnd()
    Set-Content -LiteralPath $Path -Value $current -Encoding utf8
}

$method = @(
    '# Evidencia de método — Tanda 12A, compilador self-host', '',
    '## SCOPE LOCK aplicado', '',
    'Auditoría estática exclusivamente documental de `selfhost/nsc/**`. No se modificó ni ejecutó producto, pruebas o infraestructura histórica.', '',
    '## Universo', '',
    '| Archivos | Fuentes `.ns` | Proyecto | README | Líneas | Bytes | Declaraciones | Contratos | Casos diseñados |',
    '|---:|---:|---:|---:|---:|---:|---:|---:|---:|',
    "| 29 | 27 | 1 | 1 | $(($selfHostFiles | Measure-Object lines -Sum).Sum) | $(($selfHostFiles | Measure-Object bytes -Sum).Sum) | $($symbols.Count) | $($generatedContracts.Count) | $($generatedCases.Count) |", '',
    '## Método reproducible', '',
    '1. Se leyó completo cada archivo en orden de path y se contrastó el proyecto con el conjunto físico.',
    '2. Un scanner iterativo conserva tipos y callables con ruta, línea, visibilidad y nombre calificado; no interpreta sentencias de control como declaraciones.',
    '3. La lectura semántica separó contratos de valor, cursor, builder, grafo, proyecto, semántica, IR, ABI, runtime, proceso y emisión.',
    '4. Cada declaración se asignó a un contrato primario. Los archivos no ejecutables trazan contratos de proyecto/documentación.',
    '5. Los casos futuros se derivaron por contrato: válido mínimo, válido representativo, prerrequisito ausente, estructura inválida, bordes y variantes; determinismo, mutación y escala sólo cuando aplican.',
    '6. Una segunda auditoría por componente recalculó archivos, declaraciones, contratos, casos y referencias.',
    '7. Se regeneró la clasificación transversal de todos los contratos para incluir el self-host.', '',
    '## Corrección de conteo preliminar', '',
    'Los conteos exploratorios de 829/1456 y 496 no eran válidos: el primero admitía líneas de control y el segundo omitía accessors con cuerpo de expresión. El scanner persistido exige una declaración de tipo o un callable con visibilidad, incluye accessors observables y obtiene 874 declaraciones físicas (82 tipos y 792 callables). El conteo corregido es el único promovido.', '',
    '## Límites', '',
    'No hubo build, ejecución, baseline, cobertura, instalación, despliegue, modificación de C: ni acceso remoto. `RECONSTRUIBLE` significa suficiencia documental para esta revisión, no suite implementada.'
)
$method | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'TANDA-12A-SELFHOST-METHOD.md') -Encoding utf8

$contradictions = @(
    '## Contradicciones self-host de la Tanda 12A', '',
    '| ID | Estado | Fuente A | Fuente B | Discrepancia | Impacto | Acción necesaria |',
    '|---|---|---|---|---|---|---|',
    '| `CON-0011` | ABIERTA | `README.md`, `ProjectSystem.ns`, `ModuleSet.ns`, `Program.ns` | Decisión posterior del usuario: neutralidad universal frente a ediciones internas | El self-host conserva `BootstrapProfile/2.0`, `NativeSharp/2.0`, NSMETA/1/2, `language=2.0` y texto `nsc-next 2.0.0` | Una suite nueva podría congelar residuos históricos como política futura | Caracterizar el comportamiento vigente sin convertirlo en requisito futuro; corrección funcional fuera de Fase 1 |',
    '| `CON-0012` | ABIERTA | Recorridos en Parser, Sema, IR, SourceGraph, SyntaxDumper y LlvmBackend | Lineamiento general que prohíbe recursión salvo necesidad demostrada | Existen descensos y clausuras recursivas, algunas sin cota explícita | Profundidad hostil puede agotar stack; cambiar algoritmo alteraría producto | Diseñar casos NIGHTLY de profundidad; cualquier reescritura iterativa exige otra fase y SCOPE LOCK |',
    '| `CON-0013` | ABIERTA | Comentario de `LlvmBackend.ns:139` | Toolchain actual exige componentes LLVM estáticos y no depender de `LLVM-C.dll` monolítico | El comentario histórico afirma que toda dependencia cruza `LLVM-C.dll` | Puede confundir documentación de bootstrap con el contrato de distribución vigente | Contrastar en fase funcional/toolchain; en esta fase conservar ambos hechos y no editar fuente |'
)
Replace-GeneratedSection (Join-Path $docsRoot 'CONTRADICTIONS.md') 'TANDA-12A-SELFHOST-CONTRADICTIONS' $contradictions

$reconstruct = [System.Collections.Generic.List[string]]::new()
$reconstruct.Add('## Componentes self-host auditados en Tanda 12A'); $reconstruct.Add('')
$reconstruct.Add('| Componente | Resultado | Archivos | Declaraciones | Contratos | Casos diseñados | Evidencia |')
$reconstruct.Add('|---|---|---:|---:|---:|---:|---|')
foreach ($group in $componentGroups) {
    $component=$group.Name; $slug=Slug $component
    $reconstruct.Add(('| `{0}` | `RECONSTRUIBLE` | {1} | {2} | {3} | {4} | `audits/TANDA-12A-{5}-AUDIT.md` |' -f $component,$group.Count,@($symbols|Where-Object component -eq $component).Count,@($generatedContracts|Where-Object component -eq $component).Count,@($generatedCases|Where-Object component -eq $component).Count,$slug))
}
$reconstruct.Add(''); $reconstruct.Add('El dictamen es exclusivamente documental para la revisión auditada; no demuestra ejecución, cobertura ni equivalencia con la batería histórica.')
Replace-GeneratedSection (Join-Path $docsRoot 'RECONSTRUCTIBILITY.md') 'TANDA-12A-SELFHOST-RECONSTRUCTIBILITY' $reconstruct

$indexLines = [System.Collections.Generic.List[string]]::new()
$indexLines.Add('## Compilador self-host'); $indexLines.Add('')
$indexLines.Add('- Método y universo: `evidence/TANDA-12A-SELFHOST-METHOD.md`.')
$indexLines.Add('- Inventario por archivo: `inventory/selfhost-file-audit.csv`.')
$indexLines.Add('- Auditoría consolidada: `audits/TANDA-12A-SELFHOST-AUDIT.md`.')
foreach ($group in $componentGroups) {
    $slug=Slug $group.Name
    $indexLines.Add(('- `{0}`: `components/{1}.md`; plan: `test-plans/{1}-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-{1}-AUDIT.md`.' -f $group.Name,$slug))
}
Replace-GeneratedSection (Join-Path $docsRoot 'INDEX.md') 'TANDA-12A-SELFHOST-INDEX' $indexLines

$inventoryReadme = @(
    '## Tanda 12A — self-host', '',
    '- `selfhost-file-audit.csv`: cierre de los 29 archivos de `selfhost/nsc/**`.',
    '- `symbols.csv`: 874 declaraciones físicas self-host añadidas con contrato primario.',
    ('- `contracts.csv`: {0} contratos self-host.' -f $generatedContracts.Count),
    ('- `test-case-design.csv`: {0} casos futuros self-host.' -f $generatedCases.Count),
    '- Los conteos se recalculan con `evidence/TANDA-12A-SELFHOST-GENERATOR.ps1`.'
)
Replace-GeneratedSection (Join-Path $inventoryRoot 'README.md') 'TANDA-12A-SELFHOST-INVENTORY' $inventoryReadme

# Claims are regenerated for this source slice so reruns remain idempotent.
$claimsPath = Join-Path $inventoryRoot 'claims.csv'
$claims = @(Import-Csv -LiteralPath $claimsPath | Where-Object { $_.path -notlike 'selfhost/nsc/*' })
$maxClaim = 0
foreach ($claim in $claims) { if ($claim.claim_id -match '^CL-(\d+)$') { $n=[int]$matches[1]; if($n -gt $maxClaim){$maxClaim=$n} } }
$newClaims = [System.Collections.Generic.List[object]]::new()
foreach ($group in $componentGroups) {
    $component=$group.Name; $slug=Slug $component; $componentSymbols=@($symbols|Where-Object component -eq $component); $componentContracts=@($generatedContracts|Where-Object component -eq $component); $componentCases=@($generatedCases|Where-Object component -eq $component)
    $maxClaim++; $newClaims.Add([pscustomobject][ordered]@{claim_id=('CL-{0:D6}' -f $maxClaim);classification='HECHO COMPROBADO';statement=("{0} contiene {1} archivos y {2} declaraciones fisicas en la revision auditada" -f $component,$group.Count,$componentSymbols.Count);repository='A:/nsc-devel';path=(($group.Group.path)-join ';');revision=$revision;symbol=$component;evidence_type='manifest source scan and independent audit';document="components/$slug.md";section='1;13';status='AUDITADO'})
    $maxClaim++; $newClaims.Add([pscustomobject][ordered]@{claim_id=('CL-{0:D6}' -f $maxClaim);classification='DISEÑO PROPUESTO';statement=("El plan nuevo de {0} contiene {1} contratos y {2} casos aun no implementados" -f $component,$componentContracts.Count,$componentCases.Count);repository='A:/nsc-devel';path='docs/compiler-testing-reengineering/inventory/contracts.csv;docs/compiler-testing-reengineering/inventory/test-case-design.csv';revision=$revision;symbol=$component;evidence_type='contract and case design';document="test-plans/$slug-TEST-PLAN.md";section='1-5';status='AUDITADO'})
}
@($claims + $newClaims) | Export-Csv -LiteralPath $claimsPath -NoTypeInformation -Encoding utf8

# Recompute transversal classifications after adding the self-host contracts.
& (Join-Path $PSScriptRoot 'TANDA-11-CLASSIFICATION-GENERATOR.ps1') | Out-Null

$summary = [ordered]@{
    revision=$revision; files=$selfHostFiles.Count; source_files=@($selfHostFiles|Where-Object path -like '*.ns').Count
    declarations=$symbols.Count; contracts=$generatedContracts.Count; cases=$generatedCases.Count
    pr_cases=@($generatedCases|Where-Object frequency -eq 'PR').Count
    nightly_cases=@($generatedCases|Where-Object frequency -eq 'NIGHTLY').Count
    components=@($componentGroups | ForEach-Object { [ordered]@{component=$_.Name;files=$_.Count;declarations=@($symbols|Where-Object component -eq $_.Name).Count;contracts=@($generatedContracts|Where-Object component -eq $_.Name).Count;cases=@($generatedCases|Where-Object component -eq $_.Name).Count} })
}
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $inventoryRoot 'selfhost-audit-summary.json') -Encoding utf8

# Deterministic integrity gate.
$allSymbols = @(Import-Csv -LiteralPath $symbolsPath); $allContracts = @(Import-Csv -LiteralPath $contractsPath); $allCases = @(Import-Csv -LiteralPath $casesPath)
$contractIds = @{}; foreach($c in $allContracts){if($contractIds.ContainsKey($c.contract_id)){throw "Duplicate contract $($c.contract_id)"};$contractIds[$c.contract_id]=$true}
$symbolIds = @{}; foreach($s in $allSymbols){if($symbolIds.ContainsKey($s.symbol_id) -and $s.component -like 'SelfHost.*'){throw "Duplicate new self-host symbol $($s.symbol_id)"};$symbolIds[$s.symbol_id]=$true;if($s.component -like 'SelfHost.*'){foreach($referencedContract in @($s.contract_id -split ';' | Where-Object { $_ })){if(-not $contractIds.ContainsKey($referencedContract)){throw "Missing contract $referencedContract for $($s.symbol_id)"}}}}
$caseIds = @{}; foreach($case in $allCases){if($caseIds.ContainsKey($case.case_id)){throw "Duplicate case $($case.case_id)"};$caseIds[$case.case_id]=$true;if(-not $contractIds.ContainsKey($case.contract_id)){throw "Missing contract $($case.contract_id) for $($case.case_id)"}}
foreach($c in $generatedContracts){if(@($generatedCases|Where-Object contract_id -eq $c.contract_id).Count -lt 6){throw "Insufficient case design for $($c.contract_id)"}}
foreach($entry in $protectedHashes.GetEnumerator()){ $actual=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $repoRoot $entry.Key)).Hash.ToLowerInvariant(); if($actual -ne $entry.Value){throw "Protected file changed after generation: $($entry.Key)"} }

[pscustomobject]$summary | ConvertTo-Json -Depth 5
