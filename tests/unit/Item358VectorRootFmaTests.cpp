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
namespace { using nativesharp::ExitCode;using nativesharp::driver::Driver;using nativesharp::tests::TestContext;
void writeFile(const std::filesystem::path&p,std::string_view t){std::filesystem::create_directories(p.parent_path());std::ofstream s(p);s<<t;}
ExitCode run(std::span<const std::string_view>a,std::string&o,std::string&e){std::ostringstream os,es;auto c=Driver{}.run(a,os,es);o=os.str();e=es.str();return c;}
bool execute(const std::filesystem::path&p,const std::filesystem::path&e,std::string_view opt,std::string&d){const std::string ps=p.string(),es=e.string();const std::array<std::string_view,5>a={"--emit-project-oop-exe",ps,opt,"-o",es};std::string o,x;if(run(a,o,x)!=ExitCode::Success){d=x+o;return false;}int r=std::system(("\""+es+"\"").c_str());d=r==42?std::string{}:"process result "+std::to_string(r);return r==42;}}
int main(){TestContext tests;const auto root=std::filesystem::path(NSC_BINARY_DIR)/"item358_root_fma";std::error_code x;std::filesystem::remove_all(root,x);std::filesystem::create_directories(root,x);const std::string core=(std::filesystem::path(NSC_SOURCE_DIR)/"lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();writeFile(root/"RootFma.nsproj","<Project><PropertyGroup><Name>RootFma</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/><ProjectReference Include=\""+core+"\"/></ItemGroup></Project>");writeFile(root/"Main.ns",R"NS(
using NativeSharp.Core.Numerics;
public class Program { public static int Main() {
 Float32x4 q; q.X=(float)4;q.Y=(float)9;q.Z=(float)16;q.W=(float)25;
 Float32x4 a; a.X=(float)2;a.Y=(float)2;a.Z=(float)2;a.W=(float)2;
 Float32x4 b; b.X=(float)3;b.Y=(float)3;b.Z=(float)3;b.W=(float)3;
 Float32x4 c; c.X=(float)1;c.Y=(float)1;c.Z=(float)1;c.W=(float)1;
 Float32x4 root=Float32x4.Sqrt(q);Float32x4 reciprocal=Float32x4.ReciprocalSqrt(q);
 Float32x4 fused=Float32x4.FusedMultiplyAdd(a,b,c);
 return (int)(root.X+root.Y+root.Z+root.W+reciprocal.X+fused.X)+21;
} }
)NS");std::string d;tests.expect(execute(root/"RootFma.nsproj",root/"RootFmaO0.exe","-O0",d),"sqrt/rsqrt/FMA execute at O0 ("+d+")");d.clear();tests.expect(execute(root/"RootFma.nsproj",root/"RootFmaO2.exe","-O2",d),"sqrt/rsqrt/FMA execute at O2 ("+d+")");const std::string p=(root/"RootFma.nsproj").string();const std::array<std::string_view,2>a={"--dump-project-oop-llvm",p};std::string o,e;bool dumped=run(a,o,e)==ExitCode::Success;tests.expect(dumped&&o.find("@llvm.sqrt.v4f32")!=std::string::npos&&o.find("@llvm.fma.v4f32")!=std::string::npos,"native LLVM vector sqrt and FMA are emitted ("+e+")");int r=tests.result();if(r==0)std::filesystem::remove_all(root,x);return r;}
