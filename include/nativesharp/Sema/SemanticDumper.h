#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Sema/SemanticModel.h"

#include <iosfwd>

namespace nativesharp::sema
{
class SemanticDumper final
{
public:
    void dump(const ast::AstNode* root, const SemanticModel& model, std::ostream& output) const;
    static void dumpConstant(const ConstantValue& value, std::ostream& output);

private:
    void dumpNode(
        const ast::AstNode* node,
        const SemanticModel& model,
        std::ostream& output,
        int depth,
        const char* edge) const;
};
}
