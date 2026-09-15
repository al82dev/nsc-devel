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
namespace{using nativesharp::ExitCode;using nativesharp::driver::Driver;using nativesharp::tests::TestContext;void w(const std::filesystem::path&p,std::string_view t){std::filesystem::create_directories(p.parent_path());std::ofstream s(p);s<<t;}ExitCode run(std::span<const std::string_view>a,std::string&o,std::string&e){std::ostringstream x,y;auto c=Driver{}.run(a,x,y);o=x.str();e=y.str();return c;}bool ex(const std::filesystem::path&p,const std::filesystem::path&e,std::string_view z,std::string&d){std::string ps=p.string(),es=e.string();const std::array<std::string_view,5>a={"--emit-project-oop-exe",ps,z,"-o",es};std::string o,x;if(run(a,o,x)!=ExitCode::Success){d=x+o;return false;}int r=std::system(("\""+es+"\"").c_str());d=r==42?std::string{}:"process result "+std::to_string(r);return r==42;}}
int main(){TestContext t;auto root=std::filesystem::path(NSC_BINARY_DIR)/"item362_timestamp";std::error_code x;std::filesystem::remove_all(root,x);std::filesystem::create_directories(root,x);std::string core=(std::filesystem::path(NSC_SOURCE_DIR)/"lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();w(root/"Timestamp.nsproj","<Project><PropertyGroup><Name>Timestamp</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/><ProjectReference Include=\""+core+"\"/></ItemGroup></Project>");w(root/"Main.ns",R"NS(
using NativeSharp.Core.Runtime.CompilerServices;
public class Program { public static int Main() { ulong before=HardwareIntrinsics.ReadTimestamp(); HardwareIntrinsics.Pause(); ulong after=HardwareIntrinsics.ReadTimestamp(); return after>=before?42:1; } }
)NS");std::string d;t.expect(ex(root/"Timestamp.nsproj",root/"TimestampO0.exe","-O0",d),"timestamp executes at O0 ("+d+")");d.clear();t.expect(ex(root/"Timestamp.nsproj",root/"TimestampO2.exe","-O2",d),"timestamp executes at O2 ("+d+")");std::string p=(root/"Timestamp.nsproj").string();const std::array<std::string_view,2>a={"--dump-project-oop-llvm",p};std::string o,e;bool dump=run(a,o,e)==ExitCode::Success;t.expect(dump&&o.find("@llvm.readcyclecounter")!=std::string::npos,"LLVM cycle counter intrinsic is emitted ("+e+")");int r=t.result();if(r==0)std::filesystem::remove_all(root,x);return r;}
