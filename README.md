The xv6 address space is originally set up like this:
code
guard page (inaccessible, one page)
stack (fixed-sized, one page)
heap (grows towards the high-end of the address space)

I rearranged the address space to look more like Linux:
code
heap (grows towards the high-end of the address space)
... (gap)
stack (at end of address space; grows backward)

My implementation has a test program called stacktester
that shows it works. For more info on the tests that are 
run have a look at the source code located in the root directory. 
