

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
	int numIHTNonNULLAccess;		// The number of Non Empty Inverted Hash Table Accesses
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
		printf("Proc %d Num of Non-Empty Inverted Hash Table Access %d\n", i, procTable[i].numIHTNonNULLAccess);
		printf("Proc %d Num of Page Faults %d\n", i, procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n", i, procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		assert(procTable[i].numIHTNULLAccess + procTable[i].numIHTNonNULLAccess == procTable[i].ntraces);
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
		procTable[i].numIHTNonNULLAccess = 0; // The number of Non Empty Inverted Hash TableAccesses
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

void IPHT(struct procEntry* procTable, struct framePage* physicalMemory) {
	unsigned virtualaddr, vpn, offset, physicalAddr;
	char charr;
	int i, j;
	vpn = virtualaddr >> 12;
	int two = 32 - 12 - firstLevelBits;
	offset = virtualaddr << two;
	int nFrame = (1 << (phyMemSizeBits - PAGESIZEBITS));
	struct invertedPageTableEntry* hashTable = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry)*nFrame);
	for  (j = 0; j < nFrame; j++) {
		hashTable[j].next = NULL;
		hashTable[j].pid = -1;
	}
	while (1) {
		for (i = 0; i < numProcess; i++) {
			fscanf(procTable[i].tracefp, "%x %c", &virtualaddr, &charr);//가상 주소 한 줄 읽기
			if (feof(procTable[i].tracefp) != 0) return;
			unsigned index = (vpn + i) % nFrame;
			procTable[i].ntraces++;
			struct invertedPageTableEntry* search = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry));
			printf("새로운 i값으로 시작한당\n");
			if (hashTable[index].next == NULL) {
				printf("엔트리가 없음\n");//아무런 값도 존재하지 않음->첫번째 위치에 새 엔트리를 생성후 oldest에 값을 넣는다
				procTable[i].numIHTNULLAccess++;//엔트리가 존재하지 않았던 횟수 증가
				procTable[i].numPageFault++; //fault이므로 값을 증가시킴
				struct invertedPageTableEntry* add = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry));
				hashTable[index].next = add;
				printf("엔트리의 새 값 넣엇음. 아직 값은 안 넣음\n");
				add->pid = i;//새 엔트리에 값 넣는중
				add->virtualPageNumber = vpn;
				add->frameNumber = oldestFrame->number;//오른쪽 framenumber는 oldest의 framenumber
				add->next = NULL;// 이제 oldestFrame에 값을 넣는다.
		
					if(oldestFrame->pid == -1) {
					printf("올디스크에 값이 없음\n");//oldest에 값이 존재하지 않을 경우 바로 현재 접근하는 값을 넣고 LRU 최신화를 한다
					oldestFrame->pid = i;
					oldestFrame->virtualPageNumber = vpn;
					oldestFrame = oldestFrame->lruRight;
				}
					else {//oldest에 이미 값이 존재하면 그 값제거후 다시 넣는다.
						printf("oldest에 이미 값이 존재함\n");
						int oldPid, oldvpn;
						oldPid = oldestFrame->pid; //예전 값 저장
						oldvpn = oldestFrame->virtualPageNumber;
						//oldest의 예전 값을 지닌 리스트를 찾는다.
						struct invertedPageTableEntry* before = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry));
						before = NULL;
						printf("before을 만들어서 예전 올디스트 찾는당\n");
						while (search != NULL) {
							if (search->virtualPageNumber == oldvpn && search->pid == oldPid) break; //리스트를 찾으면 나옴
							before = search;
							search = search->next;
						}
						printf("예전 올디스트 찾았당\n");
						if (before = NULL) free(search); //가장 첫번째 리스트에 존재하므로 그것만 삭제
						else if (search->next == NULL) { //가장 마지막에 존재하므로 그 전 리스트를 종점으로 하고 삭제
							free(search);
							before->next = NULL;
						}
						else { //찾은 엔트리가 중간에 존재할 경우
							before->next = search->next;
							free(search);
						}
						//원래 oldestFrame에 존재했던 값을 해시테이블에서 제거함. 이제 LRU최신화 작업을 한다
						oldestFrame = oldestFrame->lruRight; //LRU최신화
					}

			}

			else {
				printf("하나 이상 엔트리 존재\n");
				//하나 이상의 엔트리가 존재하므로 HIt인지 탐색 시작
				procTable[i].numIHTNonNULLAccess++;
				while (search != NULL) {
					procTable[i].numIHTConflictAccess++; //탐색 횟수 증가
					if (search->virtualPageNumber == vpn && search->pid == i) break;
					search = search->next;
				}
				if (search->virtualPageNumber == vpn && search->pid == i) {
					procTable[i].numPageHit++;
					//물리 메모리에서 현재 접근한 프레임을 가장 최근에 쓴 걸로 바꾼다.
					physicalMemory[index].lruLeft->lruRight = physicalMemory[index].lruRight;
					physicalMemory[index].lruRight->lruLeft = physicalMemory[index].lruLeft;
					oldestFrame->lruLeft->lruRight = &physicalMemory[index];
					physicalMemory[index].lruLeft = oldestFrame->lruLeft;
					physicalMemory[index].lruRight = oldestFrame;
					oldestFrame->lruLeft = &physicalMemory[index];//lru최신화
				}
				else { //엔트리가 존재하지 않으므로 fault발생, 새로운 값을 가장 앞 리스트로 넣고 lru최신화
				//entry not found
					procTable[i].numPageFault++;
					struct invertedPageTableEntry* add2 = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry));//새로운 엔트리
					search = hashTable[index].next;//원래 테이블의 가장 첫번째 엔트리
					add2->pid = i;
					add2->virtualPageNumber = vpn;
					add2->frameNumber = oldestFrame->number;
					add2->next = search;//새로운엔트리에 값을 넣음 .이제 LRU최신화 작업을 한다
					if (oldestFrame->pid != -1) {//oldest에 이미 값이 존재하면 그 값제거후 다시 넣는다.
						int oldPid, oldvpn;
						oldPid = oldestFrame->pid; //예전 값 저장
						oldvpn = oldestFrame->virtualPageNumber;
						//oldest의 예전 값을 지닌 리스트를 찾는다.
						struct invertedPageTableEntry* before = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry));
						before = NULL;
						while (search != NULL) {
							if (search->virtualPageNumber == oldvpn && search->pid == oldPid) break; //리스트를 찾으면 나옴
							before = search;
							search = search->next;
						}
						if (before = NULL) free(search); //가장 첫번째 리스트에 존재하므로 그것만 삭제
						else if (search->next == NULL) { //가장 마지막에 존재하므로 그 전 리스트를 종점으로 하고 삭제
							free(search);
							before->next = NULL;
						}
						else { //찾은 엔트리가 중간에 존재할 경우
							before->next = search->next;
							free(search);
						}
						//원래 oldestFrame에 존재했던 값을 해시테이블에서 제거함. 이제 LRU최신화 작업을 한다
						oldestFrame = oldestFrame->lruRight; //LRU최신화
					}
					else { //oldest에 값이 존재하지 않을 경우 바로 현재 접근하는 값을 넣고 LRU 최신화를 한다
						oldestFrame->pid = i;
						oldestFrame->virtualPageNumber = vpn;
						oldestFrame = oldestFrame->lruRight;
					}
				}
			}
			physicalAddr = (vpn << 30) + offset;
			printf("IHT procID %d traceNumber %d virtual addr %x pysical addr %x\n", i, procTable[i].ntraces, virtualaddr, physicalAddr);
		}
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
	initProcTable(procTable, numProcess, argv); //프로세스테이블 초기화'
	initPhyMem(physicalMemory, nFrame);//프로세스 테이블과 물리 메모리 만들기
	IPHT(procTable,physicalMemory);
	invertedPageVMSim(procTable, physicalMemory, nFrame);
}

		

