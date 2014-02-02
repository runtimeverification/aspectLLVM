This directory needs to reside in the llvm source tree, directly under the 
  lib/Transforms directory.
Assuming llvm was already built from those sources, just run
  make
to buid the shared library.  The result will be generated with the name
  LLVMAOP.so
or the corresponding extension for your OS, in the directory
  Release+Asserts/lib/
under the sources root directory.

To use it, load the shared library with the opt tool and use the -aop
option which is made available by this pass.  
In Linux, assuming the path to LLVMAOP.so is included in the library path,
one can use:
  opt -load LLVMAOP.so -aop < input.bc > output.bc

This command assumes the existence of an 'aspect.map' file in the current 
directory containing lines of the form:
  <when> <what> <fname> call <hname>
where <when> ::= before | after
and <what> ::= executing | calling
currently after executing is not supported.
<hname> is supposed to have the same signature as <fname> except that its 
return type is void and, for after calling instrumentation point, the 
returned value is its first argument.
  The instrumented bitcode is saved into output.bc
