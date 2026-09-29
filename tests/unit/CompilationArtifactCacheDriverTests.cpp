#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <sstream>
#include <string_view>
#include <vector>

namespace
{
nativesharp::ExitCode run(
    const std::vector<std::string_view>& arguments,
    std::ostringstream& output,
    std::ostringstream& error)
{
    return nativesharp::driver::Driver{}.run(arguments, output, error);
}
}

int main()
{
    using nativesharp::ExitCode;
    nativesharp::tests::TestContext tests;

    {
        std::ostringstream output;
        std::ostringstream error;
        tests.expect(run({}, output, error) == ExitCode::Success &&
                output.str().find("--artifact-cache=<absolute-directory>") != std::string::npos &&
                output.str().find("--artifact-cache-max-mb=N") != std::string::npos,
            "help documents verified content-addressed object reuse");
    }
    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {
            "--emit-project-oop-lib", "input.nsproj", "-o", "output.lib",
            "--artifact-cache-max-mb=128"};
        const ExitCode result = run(arguments, output, error);
        tests.expect(result == ExitCode::CommandLineError &&
                error.str().find("requires --artifact-cache") != std::string::npos,
            "a cache capacity cannot silently apply without a cache root (exit " +
                std::to_string(static_cast<int>(result)) + "): " + error.str());
    }
    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {
            "--emit-project-oop-exe", "input.nsproj", "-o", "output.exe",
            "--artifact-cache=relative/cache"};
        tests.expect(run(arguments, output, error) == ExitCode::CommandLineError &&
                error.str().find("requires an absolute directory") != std::string::npos,
            "relative artifact-cache directories are rejected before compilation");
    }
    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {
            "--version", "--artifact-cache=A:/cache"};
        tests.expect(run(arguments, output, error) == ExitCode::CommandLineError &&
                error.str().find("supported when emitting") != std::string::npos,
            "artifact cache cannot silently attach to a non-codegen action");
    }
    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {
            "--emit-project-oop-lib", "input.nsproj", "-o", "output.lib",
            "--artifact-cache=A:/one", "--artifact-cache=A:/two"};
        tests.expect(run(arguments, output, error) == ExitCode::CommandLineError &&
                error.str().find("duplicate artifact-cache") != std::string::npos,
            "duplicate artifact-cache policy is rejected deterministically");
    }
    return tests.result();
}
