#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode; using nativesharp::driver::Driver; using nativesharp::tests::TestContext;
void writeFile(const std::filesystem::path& path, std::string_view text)
{ std::filesystem::create_directories(path.parent_path()); std::ofstream stream(path); stream << text; }
ExitCode run(std::span<const std::string_view> args, std::string& outText, std::string& errText)
{ std::ostringstream out; std::ostringstream err; const auto code = Driver{}.run(args, out, err); outText=out.str(); errText=err.str(); return code; }
bool execute(const std::filesystem::path& project, const std::filesystem::path& executable,
    std::string_view optimization, std::string& diagnostic)
{
    const std::string p=project.string(), e=executable.string(); const std::array<std::string_view,5> args={"--emit-project-oop-exe",p,optimization,"-o",e};
    std::string out,err; if(run(args,out,err)!=ExitCode::Success){diagnostic=err+out;return false;}
    const int result=std::system(("\""+e+"\"").c_str()); diagnostic=result==42?std::string{}:"process result "+std::to_string(result); return result==42;
}
}
int main()
{
    TestContext tests; const auto root=std::filesystem::path(NSC_BINARY_DIR)/"item356_shuffle"; std::error_code fsError;
    std::filesystem::remove_all(root,fsError); std::filesystem::create_directories(root,fsError);
    const std::string core=(std::filesystem::path(NSC_SOURCE_DIR)/"lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
    writeFile(root/"Shuffle.nsproj","<Project><PropertyGroup><Name>Shuffle</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/><ProjectReference Include=\""+core+"\"/></ItemGroup></Project>");
    writeFile(root/"Main.ns",R"NS(
using NativeSharp.Core.Numerics;
public class Program { public static int Main() {
    Float32x4 a; a.X=(float)1; a.Y=(float)2; a.Z=(float)3; a.W=(float)4;
    Float32x4 b; b.X=(float)10; b.Y=(float)20; b.Z=(float)30; b.W=(float)40;
    Float32x4 reversed=Float32x4.SwizzleWzyx(a);
    Float32x4 mixed=Float32x4.ShuffleLowHigh(a,b);
    return (int)(reversed.X+reversed.W+mixed.X+mixed.Y+mixed.Z+mixed.W)-36;
} }
)NS");
    std::string diagnostic;
    tests.expect(execute(root/"Shuffle.nsproj",root/"ShuffleO0.exe","-O0",diagnostic),"shuffle executes at O0 ("+diagnostic+")");
    diagnostic.clear(); tests.expect(execute(root/"Shuffle.nsproj",root/"ShuffleO2.exe","-O2",diagnostic),"shuffle executes at O2 ("+diagnostic+")");
    const std::string p=(root/"Shuffle.nsproj").string(); const std::array<std::string_view,2> args={"--dump-project-oop-llvm",p};
    std::string out,err; const bool dumped=run(args,out,err)==ExitCode::Success;
    tests.expect(dumped && out.find("shufflevector <4 x float>")!=std::string::npos,"swizzle and shuffle use LLVM shufflevector ("+err+")");
    const int result=tests.result(); if(result==0)std::filesystem::remove_all(root,fsError); return result;
}
