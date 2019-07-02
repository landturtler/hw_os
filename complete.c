#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define PAGESIZEBITS 12			// page size = 4Kbytes
#define VIRTUALADDRBITS 32		// virtual address space size = 4Gbytes
#define DEBUG 0
struct pageTableEntry {
	int level;				// page table level (1 or 2)
	char valid;
	struct pageTableEntry *secondLevelPageTable;	// valid if this entry is for the first level page table (level = 1)
	int frameNumber;								// valid if this entry is for the second level page table (level = 2)
};

struct framePage {
	int number;			// frame number
	int pid;			// Process id that owns the frame
	unsigned int virtualPageNumber;			// virtual page number using the frame
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

unsigned int firstLevelBits, phyMemSizeBits, numProcess, nfirsttable;
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
			procTable[i].firstLevelPageTable[j].secondLevelPageTable = NULL;
			//procTable[i].firstLevelPageTable[j].frameNumber = -1;
		}
	}
}

void multiLevelTable(struct procEntry* procTable, struct framePage* physicalMemory, int secondLevelBits) {
	int i, j;
	unsigned  virtualaddr, physicalAddr, firstaddr, secondaddr, vpn, offset;
	char charr;
	struct framePage *temp;

	while (1) {
		if (0) {
			printf("while문 시작\n");
		}
		for (i = 0; i < numProcess; i++) {
			//printf("for문 시작%d\n", i);
			//if ((nbyte = fscanf(proc[i].tracefp, "%x %c", &addr, &character)) < 0) break;
			fscanf(procTable[i].tracefp, "%x %c", &virtualaddr, &charr);//가상 주소 한 줄 읽기
			//if (nbyte == EOF) break;//원래는 가장 아래에 있었음
			if (feof(procTable[i].tracefp) != 0) return;
			//printf("feof조건문 통과 \n");
			procTable[i].ntraces++;
			firstaddr = virtualaddr >> (12 + secondLevelBits);//첫번째 레벨 주소
			//firstaddr >>(32-firstLevelBits);
			secondaddr = (virtualaddr << firstLevelBits) >> (firstLevelBits + 12);
			vpn = virtualaddr >> 12;
			offset = (virtualaddr << (firstLevelBits + secondLevelBits)) >> (firstLevelBits + secondLevelBits);
			//printf("offset까지 정함\n");//이부분 헷갈림
			/////난원래 이거엿음if (procTable[i].firstLevelPageTable[firstaddr].valid == '0') {//secondpagetable을 만들어야함
			//세컨페이지 없으니까 만들쟈
			//printf("%d, %x, %d\n", secondLevelBits, virtualaddr, firstaddr);
			if (procTable[i].firstLevelPageTable[firstaddr].valid == '0') {
							//printf("1111\n");

				//printf("세컨레벨 만들자 \n");
				/////procTable[i].firstLevelPageTable[firstaddr].valid = '1';
				//printf("valid값 바꿈 \n");
				procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry)*(1 << secondLevelBits));
				procTable[i].num2ndLevelPageTable++;
				procTable[i].firstLevelPageTable[firstaddr].valid = '1';
				//printf("이제 문제의 두번째 레벨 초기화 \n");
				/*
				for (j = 0; j < (1 << secondLevelBits); j++) {
					procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[j].valid = '0';
					procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[j].level = 2;
					procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[j].frameNumber = -1;
					procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[j].secondLevelPageTable = NULL;
					//printf("세컨레벨 초기화햇당\n");
				}
				*/
			}
			//주소가 벨리드
			if (procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].valid == '1')
			{	
				//printf("111\n");
				procTable[i].numPageHit++;//이케하묜안대?
				int index = procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].frameNumber;
				if(&physicalMemory[index] == oldestFrame)
					oldestFrame = oldestFrame->lruRight;
				else{
					physicalMemory[index].lruRight->lruLeft = physicalMemory[index].lruLeft;
					physicalMemory[index].lruLeft->lruRight = physicalMemory[index].lruRight;
					physicalMemory[index].lruLeft = oldestFrame->lruLeft;
					physicalMemory[index].lruRight = oldestFrame;
					oldestFrame->lruLeft->lruRight = &physicalMemory[index];
					oldestFrame->lruLeft = &physicalMemory[index];
				}
			}

			else {
				//pagefault
			 	//printf("두번째에 원하는 값이 없. oldest를 넣자 \n");
				procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].valid = '1';
				procTable[i].numPageFault++;
				procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].frameNumber = oldestFrame->number;
				//procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].frameNumber  = oldestFrame->number;
				if (oldestFrame->pid == -1) {//메모리 비어있다
					//printf("222\n");
					//procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr]
					oldestFrame->pid = i;
					oldestFrame->virtualPageNumber = vpn;
					oldestFrame = oldestFrame->lruRight;
				}
				//메모리가 차있다
				else {
					//printf("333\n");

					//기존꺼 인벨리드 시키고 할당한다.
					unsigned int oldFirstAddr = oldestFrame->virtualPageNumber >> secondLevelBits;
					unsigned int oldSecondAddr = (oldestFrame->virtualPageNumber << (firstLevelBits + 12)) >> (firstLevelBits + 12);
					int oldPid = oldestFrame->pid;
					procTable[oldPid].firstLevelPageTable[oldFirstAddr].secondLevelPageTable[oldSecondAddr].valid = '0';
					//procTable[oldPid].firstLevelPageTable[oldFirstAddr].secondLevelPageTable[oldSecondAddr].frameNumber = -1;
					oldestFrame->pid = i;
					oldestFrame->virtualPageNumber = vpn;
					oldestFrame = oldestFrame->lruRight;
					//procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].frameNumber = oldestFrame->number;
					//printf("이부분 잘 되는지 의심 \n");
					//temp = (oldestFrame->lruRight)->lruRight;
					//oldestFrame->lruLeft = oldestFrame;
					//oldestFrame = oldestFrame->lruRight;v
					//oldestFrame->lruRight = temp;
					//printf("다행히 잘 되었당 \n");
					////////////여기까지
				}
			}


			
			//printf("물리주소 구하자 \n"); 
			unsigned int tempAddr = procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].frameNumber;
			unsigned int physicalAddr = tempAddr << 12;
			physicalAddr += offset;
			//printf("2Level procID %d traceNumber %d virtual addr %x pysical addr %x\n", i, procTable[i].ntraces, virtualaddr, physicalAddr);
		}
		//if (feof(procTable[i].tracefp)) break;
		//if (nbyte == EOF) break;
	}
}

void IPHT(struct procEntry* procTable, struct framePage* physicalMemory) {
	unsigned int virtualaddr, vpn, offset, physicalAddr;
	char charr;
	int i, j;
	unsigned int two = 32 - 12 - firstLevelBits;
	int nFrame = (1 << (phyMemSizeBits - PAGESIZEBITS));
	if (DEBUG) {
		printf("bbbb\n");
	}

	struct invertedPageTableEntry* hashTable = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry)*nFrame);
	if (DEBUG) {
		printf("hashtable 생성\n");
	}
	for (j = 0; j < nFrame; j++) {
		hashTable[j].next = NULL;
		hashTable[j].pid = -1;
	}
	if (DEBUG) {
		printf("dddd\n");
	}
	while (1) {
		if (DEBUG) {
			printf("while문 시작\n");
		}
		for (i = 0; i < numProcess; i++) {
			if (DEBUG) {
				printf("for문 시작");
			}
			fscanf(procTable[i].tracefp, "%x %c", &virtualaddr, &charr);//가상 주소 한 줄 읽기
			if (feof(procTable[i].tracefp) != 0) {
				if (DEBUG) {
					printf("다읽었다\n");
				}
				return;//끝나면 리턴}
			}
			//printf("%x", virtualaddr);
			vpn = virtualaddr >> 12; //vpn구하기
			offset = (virtualaddr << (firstLevelBits + two)) >> (firstLevelBits + two); //offset구하기
			unsigned int index = (vpn + i) % nFrame;//프로세스i의 인덱스
			procTable[i].ntraces++;
			if (DEBUG) {
				printf("주소 읽어오기 성공\n");
				printf("%x, %x, %x", vpn, offset, index);
			}

			if (hashTable[index].next == NULL) {//아무런 값도 존재하지 않음->첫번째 위치에 새 엔트리를 생성후 oldest에 값을 넣는다
				if (DEBUG) {
					//printf("해시테이블에 아무것도 없을 때\n");
					printf("엔트리가 없음\n");
				}
				procTable[i].numIHTNULLAccess++;//엔트리가 존재하지 않았던 횟수 증가
				procTable[i].numPageFault++; //fault이므로 값을 증가시킴
				hashTable[index].next = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry) * 1);
				hashTable[index].next->pid = i;//새 엔트리에 값 넣는중;
				hashTable[index].next->virtualPageNumber = vpn;
				hashTable[index].next->frameNumber = oldestFrame->number;
				hashTable[index].next->next = NULL;
				physicalAddr = oldestFrame->number;
				//printf("%x", physicalAddr);
				// 이제 oldestFrame에 새로운 값을 넣는다.
				if (oldestFrame->pid == -1) {//oldest에 이미 값이 존재하면 그 값제거후 다시 넣는다.
					if (DEBUG) {
						printf("oldest에 값이 없음.걍 바로 넣으셈\n");
					}
					//oldestFrame->number = oldestFrame->number;
					oldestFrame->pid = i;
					oldestFrame->virtualPageNumber = vpn;
					oldestFrame = oldestFrame->lruRight; //LRU최신화	
				}
				//이미 프레임이 차있을 경우
				else {
					if (DEBUG) {
						printf("oldest에 값이 존재. 빼고 넣어라\n");
					}
					unsigned int oldPid, oldvpn, oldindex;
					oldPid = oldestFrame->pid; //예전 값 저장
					oldvpn = oldestFrame->virtualPageNumber;
					oldindex = (oldvpn + oldPid) % nFrame;
					//oldest의 예전 값을 지닌 리스트를 찾는다.
					struct invertedPageTableEntry* search3;
					search3 = hashTable[oldindex].next;
					struct invertedPageTableEntry* before2;
					before2 = NULL;
					if (DEBUG) {
						printf("while문 전\n");
					}
					while (1) {
						if ((search3->virtualPageNumber == oldvpn) && (search3->pid == oldPid))
							break; //리스트를 찾으면 나옴
						if (DEBUG) {
							printf("while 나가는 조건 실패\n");
						}
						before2 = search3;
						search3 = search3->next;
						if (search3 == NULL) {
							printf("널값으로 나옴\n");
							break;
						}
					}

					if (DEBUG) {
						printf("while문 나옴\n");
					} //로직 다시짜기
					  //마지막 엔트리라면
					if (search3->next == NULL) {
						if(before2 == NULL){
								hashTable[oldindex].next = NULL;
								//free(search2);
							}
							else if (before2->next == NULL) {
								hashTable[oldindex].next = NULL;
								//free(search2);
								if (DEBUG) {
									printf("유일한 엔트리 제거2\n");
								}
							}//뒤에 무언가가 더 있으면
							else {
								//if((search2->next ==NULL) && (before->next !=NULL)) {
								before2->next = NULL;
								//free(search2);
								if (DEBUG) {
									printf("마지막 엔트리 제거2\n");
								}
							}
						if (DEBUG) {
							printf("serach의 다음값이 NULL일때");
						}
					}
					//엔트리가 여러개임	
					else {
						if(before2 == NULL){
							hashTable[oldindex].next = search3->next;
							free(search3);
						}
						else if (before2->next == NULL) { //첫번째에 존재
							hashTable[oldindex].next = search3->next;
							free(search3);
							if (DEBUG) {
								printf("처음 엔트리 제거2\n");
							}
						}
						else {
							before2->next = search3->next;
							free(search3);
							if (DEBUG) {
								printf("중간 엔트리 제거\n");
							}
						}
					}

					if (DEBUG) {
						printf("기존 값 제거 완료\n");
					}
					//이제 oldest에 값이 없어졌으므로 값을 넣는다
					oldestFrame->pid = i;
					oldestFrame->virtualPageNumber = vpn;
					oldestFrame = oldestFrame->lruRight; //LRU최신화
														 //////////////////여까지
				}
			}

			//이미 차있는 해시 테이블 일 경우
			else {
				if (DEBUG) {
					printf("하나 이상 엔트리 존재\n"); //하나 이상의 엔트리가 존재하므로 HIt인지 탐색 시작
				}
				struct invertedPageTableEntry* search;// = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry));
				search = hashTable[index].next;
				//procTable[i].numIHTConflictAccess++;
				procTable[i].numIHTNonNULLAccess++;//빈 엔트리가 아닌 테이블 접근
				while (search != NULL) {
					procTable[i].numIHTConflictAccess++; //탐색 횟수 증가
					if((search->virtualPageNumber == vpn) && (search->pid == i)){
						break;
					}
					search = search->next;
				}
				if (search == NULL) {//내가 찾는 엔트리가 없다
					if (DEBUG) {
						printf("fault\n"); //엔트리가 존재하지 않으므로 pagefault발생, 새로운 값을 가장 앞 리스트로 넣고oldest프레임에 넣는다 lru최신화
					}
					if (DEBUG) {
						printf("올디스크 바꾸기 이전 값: %d", oldestFrame->pid);
					}
					procTable[i].numPageFault++;
					//pagefault이므로 새 값을 넣는다.
					struct invertedPageTableEntry* oldfirst;
					oldfirst = hashTable[index].next;
					hashTable[index].next = (struct invertedPageTableEntry *)malloc(sizeof(struct invertedPageTableEntry));
					//hashTable[index].next->next = oldfirst;
					hashTable[index].next->pid = i;
					hashTable[index].next->virtualPageNumber = vpn;
					hashTable[index].next->frameNumber = oldestFrame->number;
					hashTable[index].next->next = oldfirst;//새로운엔트리에 값을 넣음 .이제 LRU최신화 작업을 한다
					if (DEBUG) {
						printf("oldestFrame값:%d", oldestFrame->pid);
					}
					physicalAddr = oldestFrame->number;
					//printf("%x", physicalAddr);
					if (oldestFrame->pid == -1) {
						if (DEBUG) {
							printf("올디스트에 값 없음\n");
						}
						//올디스트에 아무런 값이 없으므로 바로 넣고 최신화한다
						oldestFrame->pid = i;
						oldestFrame->virtualPageNumber = vpn;
						oldestFrame = oldestFrame->lruRight;
					}
					else {
						if (DEBUG) {
							printf("올디스트에 값 있음\n");
						}
						unsigned int oldPid, oldvpn, oldindex;
						//printf("old값 넣을거야\n");
						oldPid = oldestFrame->pid; //예전 값 저장
						oldvpn = oldestFrame->virtualPageNumber;
						oldindex = (oldPid + oldvpn) % nFrame;
						if (DEBUG) {
							printf("oldest의 예전 값을 지닌 리스트를 찾는다.\n");
						}
						struct invertedPageTableEntry* search2;// = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry));
						search2 = hashTable[oldindex].next;
						struct invertedPageTableEntry* before;// = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry));
						before = NULL;
						while (1) {
							if ((search2->virtualPageNumber == oldvpn) && (search2->pid == oldPid)) 
								break;
							before = search2;
							search2 = search2->next;
						}//////요까지
						if (DEBUG) {
							printf("while문 나옴\n");
						} 
						//마지막 엔트리인데
						if (search2->next == NULL) {
							//유일한 엔트리이면
							if(before == NULL){
								hashTable[oldindex].next = NULL;
								//free(search2);
							}
							else if (before->next == NULL) {
								hashTable[oldindex].next = NULL;
								//free(search2);
								if (DEBUG) {
									printf("유일한 엔트리 제거2\n");
								}
							}//뒤에 무언가가 더 있으면
							else {
								//if((search2->next ==NULL) && (before->next !=NULL)) {
								before->next = NULL;
								//free(search2);
								if (DEBUG) {
									printf("마지막 엔트리 제거2\n");
								}
							}
						}
						//중간에 있는 엔트리라면
						else {//뒤에 엔트리가 더 달려있음
							//첫번째 엔트리라면
							if(before == NULL){
								hashTable[oldindex].next = search2->next;
								//free(search2);
							}
							else if (before->next == NULL) { 
								hashTable[oldindex].next = search2->next;
								//free(search2);
								if (DEBUG) {
									printf("처음 엔트리 제거2\n");
								}
							}
							else {
								before->next = search2->next;
								//free(search2);
								if (DEBUG) {
									printf("중간 엔트리 제거\n");
								}
							}
						}
						if (DEBUG) {
							printf("기존 값 제거 완료\n");
						}
						oldestFrame->pid = i;
						oldestFrame->virtualPageNumber = vpn;
						oldestFrame = oldestFrame->lruRight;
						//oldestFrame = oldestFrame->lruRight;
						//이제 oldest에 값이 없어졌으므로 값을 넣는다
					}
					if (DEBUG) {
						printf("내가 찾는 엔트리가 없는 경우 끝났다\n");
					}
				}
				else {//내가 찾는 엔트리가 있다
					unsigned int find;
					find = search->frameNumber;
					procTable[i].numPageHit++;
					physicalAddr = find;
					//printf("%x", physicalAddr);
					//물리 메모리에서 현재 접근한 프레임을 가장 최근에 쓴 걸로 바꾼다.
					if(&physicalMemory[find] == oldestFrame)
						oldestFrame = oldestFrame->lruRight;
					else{
						physicalMemory[find].lruRight->lruLeft = physicalMemory[find].lruLeft;
						physicalMemory[find].lruLeft->lruRight = physicalMemory[find].lruRight;
						physicalMemory[find].lruLeft = oldestFrame->lruLeft;
						physicalMemory[find].lruRight = oldestFrame;
						oldestFrame->lruLeft->lruRight = &physicalMemory[find];
						oldestFrame->lruLeft = &physicalMemory[find];//lru최신화
					}
				}

			}

			physicalAddr = (physicalAddr << 12) + offset;
			//printf("물리주소 구함\n");
			printf("IHT procID %d traceNumber %d virtual addr %x pysical addr %x\n", i, procTable[i].ntraces, virtualaddr, physicalAddr);
			if (DEBUG) {
				printf("for문 끝낫습니당\n");
			}
			//printf("뭐가문제여\n");
		}
		if (DEBUG) {
			printf("while문 마지막\n");
		}
	}
}
//printf("물리 주소 받을 겁니당\n");


int main(int argc, char *argv[]) {
	phyMemSizeBits = atoi(argv[2]);  //물리메모리의 비트수
	numProcess = argc - 3; //프로세스 개수
	firstLevelBits = atoi(argv[1]); //첫번째 level의 비트수
	unsigned int secondLevelBits = VIRTUALADDRBITS - PAGESIZEBITS - firstLevelBits;//두번째 level의 비트수
	nfirsttable = 1 << firstLevelBits;//fistleveltable의 엔트리 개수
									  //int nsecondtable = 1 << secondLevelBits;//secondleveltable의 엔트리 개수 
	unsigned int nFrame = (1 << (phyMemSizeBits - PAGESIZEBITS)); assert(nFrame > 0);//총 프레임의 개수 
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
		printf("process %d opening %s\n",i,argv[i+3]);
	}
	printf("\nNum of Frames %d Physical Memory Size %ld bytes\n",nFrame, (1L<<phyMemSizeBits));

	printf("=============================================================\n");
	printf("The 2nd Level Page Table Memory Simulation Starts .....\n");
	printf("=============================================================\n");

	multiLevelTable(procTable, physicalMemory, secondLevelBits);
	secondLevelVMSim(procTable, physicalMemory);
	
	// initialize procTable for the inverted Page Table
	for(i = 0; i < numProcess; i++) {
		// rewind tracefiles
		rewind(procTable[i].tracefp);
	}
	
	printf("=============================================================\n");
	printf("The Inverted Page Table Memory Simulation Starts .....\n");
	printf("=============================================================\n");

	initProcTable(procTable, numProcess, argv); //프로세스테이블 초기화'
	initPhyMem(physicalMemory, nFrame);//프로세스 테이블과 물리 메모리 만들기
	IPHT(procTable, physicalMemory);
	invertedPageVMSim(procTable, physicalMemory, nFrame);
}







