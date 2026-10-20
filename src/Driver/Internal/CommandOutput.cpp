#include "CommandOutput.h"

#include "ProjectCommandSupport.h"
#include "nativesharp/Driver/CompilerConsole.h"

#include <ostream>

namespace nativesharp::driver
{
ExitCode CommandOutput::success(std::ostream &output, std::string_view message, std::string_view extra) const
{
    if (json)
    {
        output << "{\"schemaVersion\":1,\"command\":\"" << jsonEscape(command) << "\",\"success\":true";
        if (!project.empty())
            output << ",\"project\":\"" << jsonEscape(pathText(project)) << '"';
        if (!extra.empty())
            output << ',' << extra;
        output << ",\"message\":\"" << jsonEscape(message) << "\"}\n";
    }
    else
        output << localizeCompilerMessage(message) << '\n';
    return ExitCode::Success;
}

ExitCode CommandOutput::failure(std::ostream &error, std::string_view message, ExitCode code) const
{
    if (json)
        error << "{\"schemaVersion\":1,\"command\":\"" << jsonEscape(command)
              << "\",\"success\":false,\"exitCode\":" << static_cast<int>(code) << ",\"message\":\""
              << jsonEscape(message) << "\"}\n";
    else
        error << "nsc: error: " << localizeCompilerMessage(message) << '\n';
    return code;
}
} // namespace nativesharp::driver
