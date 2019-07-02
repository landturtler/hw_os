//
// Virual Memory Simulator Homework
// Two-level page table system
// Inverted page table with a hashing system 
// Student Name:
// Student Number:
//피지컬 메모리 값이랑 두번쨰 레벨 메모리에 차이 안뒀음..
//fault 조건을 pid로만 해서 문제였는지, vpn도 같이 봐야 하나
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define PAGESIZEBITS 12			// page size = 4Kbytes
#define VIRTUALADDRBITS 32		// virtual address space size = 4Gbytes

struct pageTableEntry {
	int level;				// page table level (1 or 2)
	char valid;
	struct pageTableEntry *secondLevelPageTable;	// valid if this entry is for the first level page table (level = 1)
	int frameNumber;								// valid if this entry is for the second level page table (level = 2)
};

struct framePage {
	int number;			// frame number
	int pid;			// Process id that owns the frame
	int virtualPageNumber;			// virtual page number using the frame
	struct framePage *lruLeft;	// for LRU circular doubly linked list
	struct framePage *lruRight; // for LRU circular doubly linked list
};

struct invertedPageTableEntry {
	int pid;					// process id
	int virtualPageNumber;		// virtual page number
	int frameNumber;			// frame number allocated
	struct invertedPageTableEntry *next;
};

struct procEntry {
	char *traceName;			// the memory trace name
	int pid;					// process (trace) id
	int ntraces;				// the number of memory traces
	int num2ndLevelPageTable;	// The 2nd level page created(allocated);
	int numIHTConflictAccess; 	// The number of Inverted Hash Table Conflict Accesses
	int numIHTNULLAccess;		// The number of Empty Inverted Hash Table Accesses
	int numIHTNonNULLAcess;		// The number of Non Empty Inverted Hash Table Accesses
	int numPageFault;			// The number of page faults
	int numPageHit;				// The number of page hits
	struct pageTableEntry *firstLevelPageTable;
	FILE *tracefp;
};

struct framePage *oldestFrame; // the oldest frame pointer

int firstLevelBits, phyMemSizeBits, numProcess, nfirsttable;
void initPhyMem(struct framePage *phyMem, int nFrame) {
	int i;
	for (i = 0; i < nFrame; i++) {
		phyMem[i].number = i;
		phyMem[i].pid = -1;
		phyMem[i].virtualPageNumber = -1;
		phyMem[i].lruLeft = &phyMem[(i - 1 + nFrame) % nFrame];
		phyMem[i].lruRight = &phyMem[(i + 1 + nFrame) % nFrame];
	}

	oldestFrame = &phyMem[0];

}

void secondLevelVMSim(struct procEntry *procTable, struct framePage *phyMemFrames) {
	int i;
	for (i = 0; i < numProcess; i++) {
		printf("**** %s *****\n", procTable[i].traceName);
		printf("Proc %d Num of traces %d\n", i, procTable[i].ntraces);
		printf("Proc %d Num of second level page tables allocated %d\n", i, procTable[i].num2ndLevelPageTable);
		printf("Proc %d Num of Page Faults %d\n", i, procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n", i, procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
}

void invertedPageVMSim(struct procEntry *procTable, struct framePage *phyMemFrames, int nFrame) {

	int i;
	for (i = 0; i < numProcess; i++) {
		printf("**** %s *****\n", procTable[i].traceName);
		printf("Proc %d Num of traces %d\n", i, procTable[i].ntraces);
		printf("Proc %d Num of Inverted Hash Table Access Conflicts %d\n", i, procTable[i].numIHTConflictAccess);
		printf("Proc %d Num of Empty Inverted Hash Table Access %d\n", i, procTable[i].numIHTNULLAccess);
		printf("Proc %d Num of Non-Empty Inverted Hash Table Access %d\n", i, procTable[i].numIHTNonNULLAcess);
		printf("Proc %d Num of Page Faults %d\n", i, procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n", i, procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		assert(procTable[i].numIHTNULLAccess + procTable[i].numIHTNonNULLAcess == procTable[i].ntraces);
	}
}

void initProcTable(struct procEntry* procTable, int numProcess, char* argv[]) {//이렇게 해도 되나
	int i, j;
	for (i = 0; i < numProcess; i++)
	{
		procTable[i].traceName = argv[i + 3];
		procTable[i].pid = i; // process (trace) id
		procTable[i].ntraces = 0; // the number of memory tracesr
		procTable[i].num2ndLevelPageTable = 0; // The 2nd level page created(allocated);
		procTable[i].numIHTConflictAccess = 0; // The number of Inverted Hash Table Conflict Accesses
		procTable[i].numIHTNULLAccess = 0; // The number of Empty Inverted Hash Table Accesses
		procTable[i].numIHTNonNULLAcess = 0; // The number of Non Empty Inverted Hash TableAccesses
		procTable[i].numPageFault = 0; // The number of page faults
		procTable[i].numPageHit = 0; // The number of page hits
		procTable[i].tracefp = fopen(argv[i + 3], "r");
		procTable[i].firstLevelPageTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry)*nfirsttable);
		for (j = 0; j < nfirsttable; j++) {
			procTable[i].firstLevelPageTable[j].level = 1;
			procTable[i].firstLevelPageTable[j].valid = '0';
			//procTable[i].firstLevelPageTable[j].secondLevelPageTable = NULL;
		}
	}
}

void multiLevelTable(struct procEntry* procTable, struct framePage* physicalMemory, int secondLevelBits) {
	int i, j;
	unsigned  virtualaddr, physicalAddr, firstaddr, secondaddr, vpn, offset;
	char charr;
    struct framePage *temp;
	printf("The 2nd Level Page Table Memory Simulation Starts ..... \n");
	printf("============================================================= \n");

	while (1) {
		for (i = 0; i < numProcess; i++) {
			//printf("for문 시작\n");
			//if ((nbyte = fscanf(proc[i].tracefp, "%x %c", &addr, &character)) < 0) break;
			fscanf(procTable[i].tracefp, "%x %c", &virtualaddr, &charr);//가상 주소 한 줄 읽기
			//if (nbyte == EOF) break;//원래는 가장 아래에 있었음
			if (feof(procTable[i].tracefp) != 0) return;
			//printf("feof조건문 통과 \n");
			procTable[i].ntraces++;
			firstaddr = virtualaddr >> (12 + secondLevelBits);//첫번째 레벨 주소
			secondaddr = (virtualaddr << firstLevelBits) >> (firstLevelBits + 12);
			vpn = virtualaddr >> 12;
			offset = (virtualaddr << (firstLevelBits + secondLevelBits)) >> (firstLevelBits + secondLevelBits);
			//printf("offset까지 정함\n");//이부분 헷갈림
			if (procTable[i].firstLevelPageTable[firstaddr].valid == '0') {//secondpagetable을 만들어야함
				//printf("세컨레벨 만들자 \n");
				procTable[i].firstLevelPageTable[firstaddr].valid = '1';
				//printf("valid값 바꿈 \n");
				procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry)*(1 << secondLevelBits));
				procTable[i].num2ndLevelPageTable++;
				//printf("이제 문제의 두번째 레벨 초기화 \n");
				procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].valid = '0';
				procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].level = 2;
				procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].frameNumber = -1;
				//printf("세컨레벨 초기화햇당\n");
			}
			if (procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].valid == '0') {//pagefault
				//printf("두번째에 원하는 값이 없. oldest를 넣자 \n");
				procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].valid = '1';
				procTable[i].numPageFault++;
				//procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].frameNumber  = oldestFrame->number;
				if (oldestFrame->pid != -1) {//안에 이미 있음
					int oldFirstAddr = oldestFrame->virtualPageNumber >> secondLevelBits;
					int oldSecondAddr = (oldestFrame->virtualPageNumber << firstLevelBits + 12) >> (firstLevelBits + 12);
					procTable[oldestFrame->pid].firstLevelPageTable[oldFirstAddr].secondLevelPageTable[oldSecondAddr].valid = '0';
					oldestFrame->pid = i;
					oldestFrame->virtualPageNumber = vpn;
					procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].frameNumber = oldestFrame->number;
					//printf("이부분 잘 되는지 의심 \n");
					temp = (oldestFrame->lruRight)->lruRight;
                    oldestFrame->lruLeft = oldestFrame;
					oldestFrame = oldestFrame->lruRight;
					oldestFrame->lruRight = temp;
					//printf("다행히 잘 되었당 \n");
				}
				else {//oldest에 아무것도 없음. 그걸 넣자
					oldestFrame->pid = i;
					oldestFrame->virtualPageNumber = vpn;
					procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].frameNumber = oldestFrame->number;
					//printf("oldest에넣었당\n"); 
                    temp = (oldestFrame->lruRight)->lruRight;
                    oldestFrame->lruLeft = oldestFrame;
                    oldestFrame = oldestFrame->lruRight;
                    oldestFrame->lruRight = temp;//최신화 완료

				}
			}//lru알고리즘을 if로 나눠야 하나
			else { //hit이므로 지금 접근한 frame은 가장 최신으로 돌리자
				procTable[i].numPageHit++;//이케하묜안대?
                int index = procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].frameNumber;
                (physicalMemory[index].lruRight)->lruLeft = physicalMemory[index].lruLeft;
                (physicalMemory[index].lruLeft)->lruRight = physicalMemory[index].lruRight;
                physicalMemory[index].lruRight = oldestFrame;
                physicalMemory[index].lruLeft = oldestFrame->lruLeft;
                oldestFrame->lruLeft->lruRight = &physicalMemory[index];
                oldestFrame->lruLeft = &physicalMemory[index]; 
                
			}
    
			//printf("물리주소 구하자 \n"); 
			int tempAddr = procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].frameNumber;
			int physicalAddr = tempAddr << 12;
			physicalAddr += offset;
			//printf("2Level procID %d traceNumber %d virtual addr %x pysical addr %x\n", i, procTable[i].ntraces, virtualaddr, physicalAddr);
		}
		//if (feof(procTable[i].tracefp)) break;
		//if (nbyte == EOF) break;
	}
}



	int main(int argc, char *argv[]) {
		phyMemSizeBits = atoi(argv[2]);  //물리메모리의 비트수
		numProcess = argc - 3; //프로세스 개수
		firstLevelBits = atoi(argv[1]); //첫번째 level의 비트수
		int secondLevelBits = VIRTUALADDRBITS - PAGESIZEBITS - firstLevelBits;//두번째 level의 비트수
		nfirsttable = 1 << firstLevelBits;//fistleveltable의 엔트리 개수
		//int nsecondtable = 1 << secondLevelBits;//secondleveltable의 엔트리 개수 
		int nFrame = (1 << (phyMemSizeBits - PAGESIZEBITS)); assert(nFrame > 0);//총 프레임의 개수 
		int i, j;
		if (argc < 4) {
			printf("Usage : %s firstLevelBits PhysicalMemorySizeBits TraceFileNames \n", argv[0]); exit(1);
		}
		if (phyMemSizeBits < PAGESIZEBITS) {
			printf("PhysicalMemorySizeBits %d should be larger than PageSizeBits %d \n", phyMemSizeBits, PAGESIZEBITS); exit(1);
		}
		if (VIRTUALADDRBITS - PAGESIZEBITS - firstLevelBits <= 0) {
			printf("firstLevelBits %d is too Big \n", firstLevelBits); exit(1);
		}
		// initialize procTable for two-level page table

		struct procEntry* procTable = (struct procEntry*)malloc(sizeof(struct procEntry)*numProcess);//프로세스개수 만큼의 프로세스테이블 
		initProcTable(procTable, numProcess, argv); //프로세스테이블 초기화
		struct framePage* physicalMemory = (struct framePage*)malloc(sizeof(struct framePage)*nFrame);//프레임개수만큼의물리메모리 
		initPhyMem(physicalMemory, nFrame);//프로세스 테이블과 물리 메모리 만들기

		for (i = 0; i < numProcess; i++) {
			//procTable[i].tracefp = fopen(argv[i+3],"r");
			printf("process %d opening %s \n", i, argv[i + 3]);
		}
		printf(" Num of Frames %d Physical Memory Size %ld bytes \n", nFrame, (1L << phyMemSizeBits));
		printf("============================================================= \n");

		multiLevelTable(procTable, physicalMemory, secondLevelBits);

		secondLevelVMSim(procTable, physicalMemory);

	}
void IPHT() {
		unsigned virtualaddr, vpn,offsest;
		char charr;
		vpn = virtualaddr >> 12;
	struct invertedPageTableEntry* hashTable = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry)*nFrame);
	while (1) {
		for (i = 0; i < numProcess; i++) {
			fscanf(procTable[i].tracefp, "%x %c", &virtualaddr, &charr);//가상 주소 한 줄 읽기
			if (feof(procTable[i].tracefp) != 0) return;
			unsigned index = (vpn + i) % nFrame;
			/*for (int j = 0; j < nFrame; j++) {//hashtable초기화
				hashTable[j].next = NULL;
			}*/

			if (hashTable[index].next == NULL) {//아무런 값도 존재하지 않음
				procTable[i].numIHTNULLAccess++;
				procTable[i].numPageFault++;
				struct invertedPageTableEntry* add = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry));
		hastTable[index].next = add;
		add->pid = i;//새로 만든 엔트리에 정보 저장
		add->virtualPageNumber = vpn;
		add->next = NULL;
		//if (vpnRetVal != -1 && pidRetVal != -1)  delete_invPageTableEntry(vpnRetVal, pidRetVal);
		if(oldestFrame.pid != -1) {//이미 oldest에 값이 존재
		int oldPid,oldVirtualPageNumber;
		struct invertedPageTableEntry* oldNext;
		oldPid = oldestFrame.pid;
		oldVirtualPageNumber = oldestFrame.virtualPageNumber;
		oldNext = oldestFrame->next;
		add->frameNumber = oldestFrame.frameNumber;//오른쪽 framenumber는 oldest의 framenumber
		printf("이제 기존 리스트 제거\n");
//아직 맨 처음부분으로 엔트리를 옮기지 못했음

		}

struct invertedPageTableEntry* search;
search = hashTable[index].next; 
int count;
while(next값이 NULL이 아님) {
count++;
if (search->virtualPageNumber == vpn && search->pid == i) break;
search.next = (search.next)->next;
} //나오는 경우는 찾았거나, 끝까지 갓는데 못찾았거나

if(search->virtualPageNumber == vpn && search->pid == i) {//찾음
procTable[i].numPageHit++;
procTable[i].numIHTConflictAccess +=count;
}
else {procTable[i].numPageFault++;//없으므로 page fault, oldest에 넣어야 함

		{
			struct invertedPageTableEntry* search;
			for (j = 0; j < 해당 엔트리의 개수; j++) {
				
				count++;
			}		void access_invPageTable(int vpn, int pid)
		{
			unsigned int idx = (vpn + pid) % nFrame;
			int vpnRetVal = vpn;
			int pidRetVal = pid;
			unsigned int frameNumber;

			struct invertedPageTableEntry * iter, *tempPtr;

			//no entry : 100% page fault
			if (hashTable[idx].head == NULL)
			{
				frameNumber = pageFault(&vpnRetVal, &pidRetVal);
				create_invPageTableEntry(&(hashTable[idx].head), pid, vpn, frameNumber);
				if (vpnRetVal != -1 && pidRetVal != -1)  delete_invPageTableEntry(vpnRetVal, pidRetVal);


			}

			//entry more than one : page fault or not
			else
			{
				iter = hashTable[idx].head;

				proc[pid].numIHTNonNULLAccess++;

				//search
				while (iter != NULL)
				{
					proc[pid].numIHTConflictAccess++;

					//entry found
					if (found(iter, vpn, pid))
					{
						LRUPolicy(iter->frameNumber);
						proc[pid].numPageHit++;
						return;
					}
					iter = iter->next;
				}

				//entry not found
				tempPtr = hashTable[idx].head;
				frameNumber = pageFault(&vpnRetVal, &pidRetVal);
				create_invPageTableEntry(&(hashTable[idx].head), pid, vpn, frameNumber);
				hashTable[idx].head->next = tempPtr;
				if (vpnRetVal != -1 && pidRetVal != -1)  delete_invPageTableEntry(vpnRetVal, pidRetVal);
				proc[pid].numPageFault++;
			}
		}




vpn = virtualAddr>>!2;

struct invertedPageTableEntry* hashTable = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry)*nFrame);
		for(int j=0; j<nFrame; j++) hashTable[j].next = NULL;
        unsigned index = (vpn + i) % nFrame;

if (hashTable[index].next == NULL) {//아무런 값도 존재하지 않음
	procTable[i].numIHTNULLAccess++;
	procTable[i].numPageFault++;
	struct invertedPageTableEntry* add;
	hastTable[index].next =(struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry));
	hastTable[index].next
void create_invPageTableEntry(struct invertedPageTableEntry ** next, int pid, int vpn, unsigned int frameNumber)
{
	*next = (struct invertedPageTableEntry *)malloc(sizeof(struct invertedPageTableEntry));
	(*next)->pid = pid;
	(*next)->virtualPageNumber = vpn;
	(*next)->frameNumber = frameNumber;
	(*next)->next = NULL;
}

}
void IPHT() {
struct invertedPageTableEntry* hashTable = malloc(struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry)*nFrame);

void access_invPageTable(int vpn, int pid)
{
	unsigned int idx = (vpn + pid) % nFrame;
	int vpnRetVal = vpn;
	int pidRetVal = pid;
	unsigned int frameNumber;
	
	struct invertedPageTableEntry * iter, *tempPtr;

	//no entry : 100% page fault
	if (hashTable[idx].head == NULL)
	{
		frameNumber = pageFault(&vpnRetVal, &pidRetVal);
		create_invPageTableEntry(&(hashTable[idx].head), pid, vpn, frameNumber);
		if (vpnRetVal != -1 && pidRetVal != -1)  delete_invPageTableEntry(vpnRetVal, pidRetVal);

	
	}

	//entry more than one : page fault or not
	else
	{
		iter = hashTable[idx].head;

		proc[pid].numIHTNonNULLAccess++;
	
		//search
		while (iter != NULL)
		{
			proc[pid].numIHTConflictAccess++;
			
			//entry found
			if(found(iter,vpn,pid))
			{ 
				LRUPolicy(iter->frameNumber);	
				proc[pid].numPageHit++;
				return;
			}
			iter = iter->next;
		}d
		
		//entry not found
		tempPtr = hashTable[idx].head;
		frameNumber = pageFault(&vpnRetVal, &pidRetVal);
		create_invPageTableEntry(&(hashTable[idx].head), pid, vpn, frameNumber); 
		hashTable[idx].head->next = tempPtr;
		if (vpnRetVal != -1 && pidRetVal != -1)  delete_invPageTableEntry(vpnRetVal, pidRetVal);
		proc[pid].numPageFault++;
	}
}

int found(struct invertedPageTableEntry* iter, int vpn, int pid)
{
	if(iter->virtualPageNumber == vpn && iter->pid == pid) return 1;
	else return 0;
}
*/
