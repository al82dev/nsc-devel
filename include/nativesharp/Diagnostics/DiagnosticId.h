#pragma once

#include <string_view>

namespace nativesharp::diagnostics
{
enum class DiagnosticId
{
    CannotOpenSourceFile,
    SourceFileTooLarge,

    UnexpectedCharacter,
    UnterminatedBlockComment,
    InvalidNumericLiteral,
    InvalidCharacterLiteral,
    InvalidStringLiteral,
    InvalidEscapeSequence,

    ExpectedToken,
    UnexpectedToken,
    ExpectedDeclaration,
    ExpectedExpression,
    ExpectedType,
    UnknownTypeName,
    UnknownNamespace,
    InvalidUsingTarget,
    DuplicateUsingAlias,
    AmbiguousTypeName,
    PartialTypeConflict,
    InvalidBaseType,
    InheritanceCycle,
    InaccessibleMember,
    InvalidOverride,

    UnknownIdentifier,
    DuplicateSymbol,
    PreviousDeclaration,
    ConflictingFunctionDeclaration,
    DuplicateFunctionDefinition,
    MissingFunctionDefinition,
    InvalidType,
    IncompleteType,
    TypeMismatch,
    InvalidConversion,
    InvalidOperand,
    InvalidAssignmentTarget,
    InvalidAddressOf,
    InvalidDereference,
    InvalidCall,
    InvalidArgumentCount,
    InvalidMemberAccess,
    UnknownMember,
    InvalidIndex,
    ConstantIndexOutOfBounds,
    InvalidCondition,
    InvalidReturn,
    NotAllPathsReturn,
    BreakOutsideLoop,
    ContinueOutsideLoop,
    ConstRequiresConstant,
    StaticAssertionFailed,
    GlobalRequiresConstant,
    InvalidArrayCount,
    InvalidEnumInitializer,
    InvalidSizeof,
    InvalidOffsetof,
    InvalidAllocationType,
    InvalidDeleteOperand,
    InvalidStackalloc,
    InvalidInlineAssemblyConstraint,
    InvalidInlineAssemblyClobber,
    InvalidInlineAssemblyControlFlow,
    InvalidInlineAssemblyTarget,
    InvalidInlineAssemblyPlaceholder,
    InlineAssemblyNotAvailable,
    PlacementConstructionNotAvailable,
    ConstantEvaluationTrap,
    MissingMain,
    InvalidMainSignature,
    InvalidExternCSignature,

    NS3001IncompatibleLanguageEdition,
    NS3002FeatureRequiresLanguageEdition3,
    NS3010InvalidCompilationStage,
    NS3020InvalidValueCategory,
    NS3030MissingEffectContext,
    NS3049DeferredLanguageSurface,
    NS3099InvalidConformanceManifest
};

constexpr bool isNativeSharp3Diagnostic(DiagnosticId id) noexcept
{
    switch (id)
    {
        case DiagnosticId::NS3001IncompatibleLanguageEdition:
        case DiagnosticId::NS3002FeatureRequiresLanguageEdition3:
        case DiagnosticId::NS3010InvalidCompilationStage:
        case DiagnosticId::NS3020InvalidValueCategory:
        case DiagnosticId::NS3030MissingEffectContext:
        case DiagnosticId::NS3049DeferredLanguageSurface:
        case DiagnosticId::NS3099InvalidConformanceManifest:
            return true;
        default:
            return false;
    }
}

// These names are the stable machine-facing diagnostic codes used by editor integrations. New
// IDs may be added without changing existing spellings.
constexpr std::string_view diagnosticIdName(DiagnosticId id) noexcept
{
    switch (id)
    {
#define NSC_DIAGNOSTIC_NAME(name) case DiagnosticId::name: return #name
        NSC_DIAGNOSTIC_NAME(CannotOpenSourceFile);
        NSC_DIAGNOSTIC_NAME(SourceFileTooLarge);
        NSC_DIAGNOSTIC_NAME(UnexpectedCharacter);
        NSC_DIAGNOSTIC_NAME(UnterminatedBlockComment);
        NSC_DIAGNOSTIC_NAME(InvalidNumericLiteral);
        NSC_DIAGNOSTIC_NAME(InvalidCharacterLiteral);
        NSC_DIAGNOSTIC_NAME(InvalidStringLiteral);
        NSC_DIAGNOSTIC_NAME(InvalidEscapeSequence);
        NSC_DIAGNOSTIC_NAME(ExpectedToken);
        NSC_DIAGNOSTIC_NAME(UnexpectedToken);
        NSC_DIAGNOSTIC_NAME(ExpectedDeclaration);
        NSC_DIAGNOSTIC_NAME(ExpectedExpression);
        NSC_DIAGNOSTIC_NAME(ExpectedType);
        NSC_DIAGNOSTIC_NAME(UnknownTypeName);
        NSC_DIAGNOSTIC_NAME(UnknownNamespace);
        NSC_DIAGNOSTIC_NAME(InvalidUsingTarget);
        NSC_DIAGNOSTIC_NAME(DuplicateUsingAlias);
        NSC_DIAGNOSTIC_NAME(AmbiguousTypeName);
        NSC_DIAGNOSTIC_NAME(PartialTypeConflict);
        NSC_DIAGNOSTIC_NAME(InvalidBaseType);
        NSC_DIAGNOSTIC_NAME(InheritanceCycle);
        NSC_DIAGNOSTIC_NAME(InaccessibleMember);
        NSC_DIAGNOSTIC_NAME(InvalidOverride);
        NSC_DIAGNOSTIC_NAME(UnknownIdentifier);
        NSC_DIAGNOSTIC_NAME(DuplicateSymbol);
        NSC_DIAGNOSTIC_NAME(PreviousDeclaration);
        NSC_DIAGNOSTIC_NAME(ConflictingFunctionDeclaration);
        NSC_DIAGNOSTIC_NAME(DuplicateFunctionDefinition);
        NSC_DIAGNOSTIC_NAME(MissingFunctionDefinition);
        NSC_DIAGNOSTIC_NAME(InvalidType);
        NSC_DIAGNOSTIC_NAME(IncompleteType);
        NSC_DIAGNOSTIC_NAME(TypeMismatch);
        NSC_DIAGNOSTIC_NAME(InvalidConversion);
        NSC_DIAGNOSTIC_NAME(InvalidOperand);
        NSC_DIAGNOSTIC_NAME(InvalidAssignmentTarget);
        NSC_DIAGNOSTIC_NAME(InvalidAddressOf);
        NSC_DIAGNOSTIC_NAME(InvalidDereference);
        NSC_DIAGNOSTIC_NAME(InvalidCall);
        NSC_DIAGNOSTIC_NAME(InvalidArgumentCount);
        NSC_DIAGNOSTIC_NAME(InvalidMemberAccess);
        NSC_DIAGNOSTIC_NAME(UnknownMember);
        NSC_DIAGNOSTIC_NAME(InvalidIndex);
        NSC_DIAGNOSTIC_NAME(ConstantIndexOutOfBounds);
        NSC_DIAGNOSTIC_NAME(InvalidCondition);
        NSC_DIAGNOSTIC_NAME(InvalidReturn);
        NSC_DIAGNOSTIC_NAME(NotAllPathsReturn);
        NSC_DIAGNOSTIC_NAME(BreakOutsideLoop);
        NSC_DIAGNOSTIC_NAME(ContinueOutsideLoop);
        NSC_DIAGNOSTIC_NAME(ConstRequiresConstant);
        NSC_DIAGNOSTIC_NAME(StaticAssertionFailed);
        NSC_DIAGNOSTIC_NAME(GlobalRequiresConstant);
        NSC_DIAGNOSTIC_NAME(InvalidArrayCount);
        NSC_DIAGNOSTIC_NAME(InvalidEnumInitializer);
        NSC_DIAGNOSTIC_NAME(InvalidSizeof);
        NSC_DIAGNOSTIC_NAME(InvalidOffsetof);
        NSC_DIAGNOSTIC_NAME(InvalidAllocationType);
        NSC_DIAGNOSTIC_NAME(InvalidDeleteOperand);
        NSC_DIAGNOSTIC_NAME(InvalidStackalloc);
        NSC_DIAGNOSTIC_NAME(InvalidInlineAssemblyConstraint);
        NSC_DIAGNOSTIC_NAME(InvalidInlineAssemblyClobber);
        NSC_DIAGNOSTIC_NAME(InvalidInlineAssemblyControlFlow);
        NSC_DIAGNOSTIC_NAME(InvalidInlineAssemblyTarget);
        NSC_DIAGNOSTIC_NAME(InvalidInlineAssemblyPlaceholder);
        NSC_DIAGNOSTIC_NAME(InlineAssemblyNotAvailable);
        NSC_DIAGNOSTIC_NAME(PlacementConstructionNotAvailable);
        NSC_DIAGNOSTIC_NAME(ConstantEvaluationTrap);
        NSC_DIAGNOSTIC_NAME(MissingMain);
        NSC_DIAGNOSTIC_NAME(InvalidMainSignature);
        NSC_DIAGNOSTIC_NAME(InvalidExternCSignature);
#undef NSC_DIAGNOSTIC_NAME
        case DiagnosticId::NS3001IncompatibleLanguageEdition: return "NS3001";
        case DiagnosticId::NS3002FeatureRequiresLanguageEdition3: return "NS3002";
        case DiagnosticId::NS3010InvalidCompilationStage: return "NS3010";
        case DiagnosticId::NS3020InvalidValueCategory: return "NS3020";
        case DiagnosticId::NS3030MissingEffectContext: return "NS3030";
        case DiagnosticId::NS3049DeferredLanguageSurface: return "NS3049";
        case DiagnosticId::NS3099InvalidConformanceManifest: return "NS3099";
    }
    return "UnknownDiagnostic";
}
}
