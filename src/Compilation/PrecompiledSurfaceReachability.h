#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <cstddef>
#include <deque>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::compilation
{
// Computes which declarations of a set of precompiled modules the source of one compilation can
// reach, and builds the metadata that keeps only them.
//
// The closure is conservative. A type, generic template, namespace function or namespace state is
// reached when its simple name is one of the demanded names. A reached type brings its containing
// types, all of its members, the trait implementations that name it, and every type its fields,
// bases, interfaces and member signatures name; a reached generic template brings its dependencies,
// its declaring type and every name written in its body. Text carried by the metadata (inline
// bodies, default values, constants, template bodies) adds its names to the demand. Every value
// the source can produce has a type named by some reached declaration, so every declaration the
// source can use is reached.
//
// Modules marked as full are never pruned: the compiler itself refers to standard-library
// declarations by key, without any name in the source.
class PrecompiledSurfaceReachability final
{
public:
    PrecompiledSurfaceReachability(
        std::span<const std::shared_ptr<const interop::LibraryMetadata>> modules,
        const std::vector<bool>& fullModules);

    // Adds the demanded names and closes the reachable set.
    void reach(const std::unordered_set<std::string>& names);

    // Returns the metadata of `module` that keeps only the reached declarations, or the original
    // metadata when the module is full or every declaration was reached.
    std::shared_ptr<const interop::LibraryMetadata> prunedMetadata(std::size_t module) const;

private:
    using Location = std::pair<std::size_t, std::size_t>;

    void indexModule(std::size_t module);
    void indexTraitImpls(std::size_t module);
    void drain();
    void reachName(std::string_view name);
    void reachType(std::string_view identity);
    void reachTemplate(std::string_view key);
    void processType(std::string_view identity);
    void processTemplate(std::string_view key);
    void processName(std::string_view name);
    void includeType(const Location& location);
    void includeMember(const Location& location);
    void includeTraitImpl(const Location& location);
    void includeFunction(const Location& location);
    void includeState(const Location& location);
    void includeTemplate(const Location& location);
    void scanCallable(const interop::LibraryMetadataMember& member);
    void scanParameters(std::span<const interop::LibraryMetadataParameter> parameters);
    void scanTypeKeys(std::string_view text);
    void scanSourceNames(std::string_view text);
    bool everythingReached(std::size_t module) const;

    std::vector<std::shared_ptr<const interop::LibraryMetadata>> modules_;
    std::vector<bool> fullModules_;
    std::vector<std::vector<bool>> includedTypes_;
    std::vector<std::vector<bool>> includedMembers_;
    std::vector<std::vector<bool>> includedTraitImpls_;
    std::vector<std::vector<bool>> includedFunctions_;
    std::vector<std::vector<bool>> includedStates_;
    std::vector<std::vector<bool>> includedTemplates_;
    std::unordered_map<std::string_view, std::vector<Location>> typesByIdentity_;
    std::unordered_map<std::string_view, std::vector<Location>> typesByName_;
    std::unordered_map<std::string_view, std::vector<Location>> membersByDeclaringType_;
    std::unordered_map<std::string_view, std::vector<Location>> traitImplsByType_;
    std::vector<Location> unconditionalTraitImpls_;
    std::unordered_map<std::string_view, std::vector<Location>> functionsByName_;
    std::unordered_map<std::string_view, std::vector<Location>> statesByName_;
    std::unordered_map<std::string_view, Location> templateByKey_;
    std::unordered_map<std::string_view, std::vector<Location>> templatesByName_;
    std::unordered_map<std::string_view, std::vector<Location>> specializationsByType_;
    std::unordered_map<std::string_view, std::vector<std::string_view>> specializedTypesByTemplateName_;
    std::unordered_set<std::string_view> reachedNames_;
    std::unordered_set<std::string_view> reachedTypes_;
    std::unordered_set<std::string_view> reachedTemplates_;
    std::vector<std::string_view> pendingNames_;
    std::vector<std::string_view> pendingTypes_;
    std::vector<std::string_view> pendingTemplates_;
    // Owns the strings that the views above point to when they are not part of a metadata record.
    std::deque<std::string> storage_;
};
} // namespace nativesharp::compilation
