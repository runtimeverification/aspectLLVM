all: test.bc

aspect.bc: 
	clang aspect.c -o aspect.bc -c -emit-llvm

test0.bc: 
	clang test.c -o test0.bc -c -emit-llvm

test_instr.bc: test0.bc aspect.map
	opt -load LLVMAOP.so -aop < test0.bc >test_instr.bc

test.bc: aspect.bc test_instr.bc 
	llvm-link  aspect.bc test_instr.bc -o test.bc
	
clean:
	rm -f *.bc 
