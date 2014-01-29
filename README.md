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
directory containing space-separated pairs of method names 
  fname hname
and will locate all functions named fname in input.bc inserting as the first
instruction in their body a call to hname.  hname is supposed to be a void
function with no arguments.  The instrumented bitcode is saved into 
output.bc
