#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "../my_vm.h"

void* try_our_malloc(int num_bytes){
        void* va = t_malloc(num_bytes);
        printf("Tried mallocing %d bytes. Received VA: %p\n", num_bytes, va);
        return va;
        
}

int main(){
        printf("Keith and Ben's test\n");

        void* malloc1 = try_our_malloc(4097);
        t_free(malloc1, 4097);


}

