project.name = llvmdc

package = newpackage()
package.name = "idgen"
package.kind = "exe"
package.language = "c++"
package.files = { "dmd/idgen.c" }
package.buildoptions = { "-x c++" }
package.postbuildcommands = { "./idgen", "mv -f id.c id.h dmd" }

package = newpackage()
package.name = "impcnvgen"
package.kind = "exe"
package.language = "c++"
package.files = { "dmd/impcnvgen.c" }
package.buildoptions = { "-x c++" }
package.postbuildcommands = { "./impcnvgen", "mv -f impcnvtab.c dmd" }

package = newpackage()
package.bindir = "bin"
package.name = "llvmdc"
package.kind = "exe"
package.language = "c++"
package.files = { matchfiles("dmd/*.c"), matchfiles("gen/*.c") }
package.excludes = { "dmd/idgen.c", "dmd/impcnvgen.c" }
package.buildoptions = { "-x c++", "`llvm-config --cxxflags`" }
package.linkoptions = { "`llvm-config --libs native bitwriter bitreader`", "`llvm-config --ldflags`" }
package.defines = { "IN_LLVM", "_DH" }
package.config.Release.defines = { "LLVMD_NO_LOGGER" }
package.config.Debug.buildoptions = { "-g" }
--package.targetprefix = "llvm"
package.includepaths = { ".", "dmd" }
--package.postbuildcommands = { "cd runtime; ./build.sh; cd .." }
package.links = { "gc" }
