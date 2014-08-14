This directory needs to reside in the llvm source tree, directly under the 
  `lib/Transforms directory`.
Assuming llvm was already built from those sources, just run
  `make`
to buid the shared library.  The result will be generated with the name
  `LLVMAOP.so`
or the corresponding extension for your OS, in the directory
  `Release+Asserts/lib/`
under the sources root directory.

To use it, load the shared library with the opt tool and use the -aop
option which is made available by this pass.  
In Linux, assuming the path to `LLVMAOP.so` is included in the library path,
one can use:

    opt -load LLVMAOP.so -aop input.bc -o output.bc

The aspectLLVM tool expects a configuration file describing the pointcuts
and the function calls to be inserted there.  The location of the file can be 
specified using the `RVX_ASPECT` environment variable.   If that variable is
not set, the tool will look for an `aspect.map` file in the current directory.

The aspect file is expected to contain lines of the form:

    <when> <what> <fname> call <hname>

where `<when> ::= before | after | instead-of`
and `<what> ::= executing | calling`
(currently "executing" is only supported for "before")

`<hname>` is supposed to have the same signature as `<fname>` except that its 
return type is void and, for after calling instrumentation point, it has 
an additional first argument to hold the return value.
The instrumented bitcode is saved into `output.bc` (which can be the same as
 `input.bc`)

