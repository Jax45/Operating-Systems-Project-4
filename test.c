#include <stdlib.h>
#include <stdio.h>
#include "customStructs.h"
#include "bitMap.h"

int main(void) {

	unsigned char bitMap[3];
	memset(bitMap, '\0', sizeof(bitMap));
	int lastPid = -1;

	//start of loop
	int i = 0;
	for( i = 0; i < 30; i++){
		if((lastPid = openBit(bitMap,lastPid)) != -1){
			// we have an open spot in lastPid
			setBit(bitMap,lastPid);
			printf("set bit at spot %d\n",i);
		}
	
	}
	return 0;
}


/*  struct Queue* q = createQueue();
  enQueue(q, 1);
  enQueue(q, 2);
  enQueue(q, 3);
  enQueue(q, 4);

struct Node* n;
  while(sizeOfQueue(q) > 0){
	n = deQueue(q);
	printf("%d\n",n->key);
  }
  printf("size of q: %d",sizeOfQueue(q));
  
  printf("Hello World\n%d",n->key);
  	
 
	 
  return 0;
}*/
