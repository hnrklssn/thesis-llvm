# LoopVectorizationLegality

 - check if control flow can be converted to data flow (if conversion)
	- if loop body is only 1 BB, there is not control flow to flatten -> skip
	- contains switch statement -> abort
	for each BB:
	- check if needs predication
		- does not dominate latch block -> needs predication
	- if yes, check if can be predicated. Cannot be predicated -> abort
		- contains instruction that can result in exception (not UB) -> cannot be predicated
		- contains atomics or memory fences for load/stores -> cannot be predicated
		- if not annotated safe to parallelize and we cannot prove that we are not introducing potentially unsafe memory accesses, must mask (somewhat costly)
	- check if can convert phi nodes
		- if 
	- if no -> abort
 - check if instrs can be vectorized
	- all phis have to be int, float or pointer types
	- all phis need to have exactly two preds
	- float values produced that can be accessed outside loop are only allowed with fast math

# Terminology
- latch block: 
- allowed exit: value that can be accessed outside loop


#
- hur phi med struct/array?
- hur flera predecessors utan switch?

# förbättringsmöjligheter
- peka ut return/break som brytande control flow
- GVN: vilket funktionsanrop clobberade? vilken variabel?
- vad betyder                   

Cannot SLP vectorize list: vectorization was impossible with available vectorization factors 
