#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define PAGESIZEBITS 12			// page size = 4Kbytes
#define VIRTUALADDRBITS 32		// virtual address space size = 4Gbytes
#define DEBUG 1
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
			procTable[i].firstLevelPageTable[j].valid = '1';
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
			//firstaddr >>(32-firstLevelBits);
			secondaddr = (virtualaddr << firstLevelBits) >> (firstLevelBits + 12);
			vpn = virtualaddr >> 12;
			offset = (virtualaddr << (firstLevelBits + secondLevelBits)) >> (firstLevelBits + secondLevelBits);
			//printf("offset까지 정함\n");//이부분 헷갈림
			/////난원래 이거엿음if (procTable[i].firstLevelPageTable[firstaddr].valid == '0') {//secondpagetable을 만들어야함
		


			if (procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable == NULL) {
				//printf("세컨레벨 만들자 \n");
				/////procTable[i].firstLevelPageTable[firstaddr].valid = '1';
				//printf("valid값 바꿈 \n");
				procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry)*(1 << secondLevelBits));
				procTable[i].num2ndLevelPageTable++;
				//printf("이제 문제의 두번째 레벨 초기화 \n");
				for(j =0; j <(1<<secondLevelBits);j++) {
					procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[j].valid = '0';
					procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[j].level = 2;
					procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[j].frameNumber = -1;
					procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[j].secondLevelPageTable = NULL;
					//printf("세컨레벨 초기화햇당\n");
				}
			}
			if (procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].valid == '0' ) 
			 {//pagefault
				//printf("두번째에 원하는 값이 없. oldest를 넣자 \n");
				procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].valid = '1';
				procTable[i].numPageFault++;
				procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].frameNumber  = oldestFrame->number;
				//procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].frameNumber  = oldestFrame->number;
				if (oldestFrame->pid != -1) {//안에 이미 있음
					//procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr]
				unsigned int oldFirstAddr = oldestFrame->virtualPageNumber >> secondLevelBits;
					unsigned int oldSecondAddr = (oldestFrame->virtualPageNumber << firstLevelBits+12 ) >> (firstLevelBits+12 );
					procTable[oldestFrame->pid].firstLevelPageTable[oldFirstAddr].secondLevelPageTable[oldSecondAddr].valid = '0';
				procTable[oldestFrame->pid].firstLevelPageTable[oldFirstAddr].secondLevelPageTable[oldSecondAddr].frameNumber = -1;
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
				}
				else {//oldest에 아무것도 없음. 그걸 넣자
					oldestFrame->pid = i;
					oldestFrame->virtualPageNumber = vpn;
					oldestFrame = oldestFrame->lruRight;
					//procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].frameNumber = oldestFrame->number;
					//printf("oldest에넣었당\n"); 
					//temp = (oldestFrame->lruRight)->lruRight;
					//oldestFrame->lruLeft = oldestFrame;
					//oldestFrame->lruRight = temp;//최신화 완료

				}
			}//lru알고리즘을 if로 나눠야 하나
			else { //hit이므로 지금 접근한 frame은 가장 최신으로 돌리자
				procTable[i].numPageHit++;//이케하묜안대?
				int index = procTable[i].firstLevelPageTable[firstaddr].secondLevelPageTable[secondaddr].frameNumber;
				(physicalMemory[index].lruRight)->lruLeft = physicalMemory[index].lruLeft;
				(physicalMemory[index].lruLeft)->lruRight = physicalMemory[index].lruRight;
				oldestFrame->lruLeft->lruRight = &physicalMemory[index];
				oldestFrame->lruLeft = &physicalMemory[index];
				physicalMemory[index].lruRight = oldestFrame;
				physicalMemory[index].lruLeft = oldestFrame->lruLeft;
				oldestFrame = oldestFrame->lruRight;
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

void IPHT(struct procEntry* procTable,struct framePage* physicalMemory) {
    //int DEBUG = 0;
    unsigned int virtualaddr, vpn, offset, physicalAddr;
	char charr;
	int i,j;
	int two = 32 - 12 - firstLevelBits;
	int nFrame = (1 << (phyMemSizeBits - PAGESIZEBITS));
	if(DEBUG){
        printf("bbbb\n");
    }
    printf("=============================================================           \n");
      printf("The Inverted Page Table Memory Simulation Starts ..... \n");    printf("=============================================================       \n");
      
	struct invertedPageTableEntry* hashTable = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry)*nFrame);
	if(DEBUG){
        printf("cccc\n");
    }
	for (j = 0; j < nFrame; j++) {
		hashTable[j].next = NULL;
		hashTable[j].pid = -1;
	}
    if(DEBUG){
        printf("dddd\n");
    }
	while (1) {
		for (i = 0; i < numProcess; i++) {
			fscanf(procTable[i].tracefp, "%x %c", &virtualaddr, &charr);//가상 주소 한 줄 읽기
			if (feof(procTable[i].tracefp) != 0) {
			 	if(DEBUG){
            	   	printf("다읽었다\n");
				}
			 	return;//끝나면 리턴}
			 }

			vpn = virtualaddr >> 12; //vpn구하기
			offset = (virtualaddr <<(firstLevelBits + two)) >> (firstLevelBits + two); //offset구하기
			unsigned int index = (vpn + i) % nFrame;//프로세스i의 인덱스
			procTable[i].ntraces++;
			if(DEBUG){
     			printf("주소 읽어오기 성공\n");
    		}
			if(hashTable[index].next != NULL) {
				if(DEBUG){
					printf("하나 이상 엔트리 존재\n"); //하나 이상의 엔트리가 존재하므로 HIt인지 탐색 시작
				}
				struct invertedPageTableEntry* search;// = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry));
 				search = hashTable[index].next;
				procTable[i].numIHTNonNULLAccess++;//빈 엔트리가 아닌 테이블 접근
				while (search != NULL) {
					procTable[i].numIHTConflictAccess++; //탐색 횟수 증가
					if ((search->virtualPageNumber == vpn) && (search->pid == i)) break;
					search = search->next;
				}
				if(search ==NULL) {
					if(DEBUG){
						printf("fault\n"); //엔트리가 존재하지 않으므로 pagefault발생, 새로운 값을 가장 앞 리스트로 넣고oldest프레임에 넣는다 lru최신화
					}
					//entry not found
					//printf("aaa\n");
					procTable[i].numPageFault++;
					
				//==================================//이 부분을 잘 모르겟음
					struct invertedPageTableEntry* oldfirst;
					oldfirst= hashTable[index].next;
                    hashTable[index].next = (struct invertedPageTableEntry *)malloc(sizeof(struct invertedPageTableEntry));
					hashTable[index].next->next = oldfirst;
                    hashTable[index].pid = i;
					hashTable[index].virtualPageNumber = vpn;
					hashTable[index].frameNumber = oldestFrame->number;
					hashTable[index].next = oldfirst;//새로운엔트리에 값을 넣음 .이제 LRU최신화 작업을 한다
				  //struct inverteddPageEntry* makeNext = hashTable[index].next;
                //*makeNext = (struct invertedPageTableEntry *)malloc(1 * sizeof(struct invertedPageTableEntry));
                //(*makeNext)->pid = i;
                //(*makeNext)->virtualPageNumber = vpn;
                //(*makeNext)->frameNumber = FrameNumber;
                //(*makeNext)->next = NULL;
					if (oldestFrame->pid != -1) {//oldest에 이미 값이 존재하면 그 값제거후 다시 넣는다.
						if(DEBUG){
							printf("올디스트에 값 존재\n");
						}
						int oldPid, oldvpn, oldindex;
						//printf("old값 넣을거야\n");
						oldPid = oldestFrame->pid; //예전 값 저장
						oldvpn = oldestFrame->virtualPageNumber;
						oldindex = (oldPid + oldvpn) % nFrame;
						if(DEBUG){
							printf("oldest의 예전 값을 지닌 리스트를 찾는다.\n");
						}
						struct invertedPageTableEntry* search2;// = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry));
						search2 = hashTable[oldindex].next;
						struct invertedPageTableEntry* before;// = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry));
						before = NULL;
						while(1) { 
							if(search2->virtualPageNumber == oldvpn && search2->pid == oldPid) break; 
							before = search2;
							search2 = search2->next;
						}
						/*if(search2 ==NULL){ 
							if(DEBUG){
								printf("말이 되나\n");
							}
						}
						
						if(DEBUG){
							printf("비포 값 지정함\n");
						}
						
						while (search2 != NULL) {
							if (search2->virtualPageNumber == oldvpn && search2->pid == oldPid) break; //리스트를 찾으면 나옴
						
						}
						if(DEBUG){
							printf("와일문 나왓니\n");
						}*/

						//if(search2 ==NULL) {
							//printf("올디스트에 값 존재 안함\n");//oldest에 값이 존재하지 않을 경우 바로 현재 접근하는 값을 넣고 LRU 최신화를 한다
							//oldestFrame->pid = i;
							//oldestFrame->virtualPageNumber = vpn;
							//oldestFrame = oldestFrame->lruRight;
						//	if(DEBUG){
						//		printf("뭔가 이상하다\n");
						//	}
						//}

						//else { //search2가 중간에 예전 값을 찾았다.
						if(DEBUG) {
							printf("while문 나옴\n");
						} //로직 다시짜기
							if((search2->next == NULL) && (before->next = NULL)) {//유일한 엔트리
								hashTable[oldindex].next =NULL;
								free(search2);
								if(DEBUG) {
							printf("유일한 엔트리 제거\n");}
							}//뒤에 무언가가 더 있으면
							else if(search2->next ==NULL && before->next !=NULL) {
								before->next = NULL;
								free(search2);
								if(DEBUG) {
								printf("마지막 엔트리 제거\n");
								}
							}		
							else if (before->next == NULL && search2->next !=NULL) { //가장 마지막에 존재하므로 그 전 리스트를 종점으로 하고 삭제
								hashTable[oldindex].next = search2->next;
								free(search2);
								if(DEBUG) {
							printf("처음 엔트리 제거\n");}
							}
							else {
								before->next = search2->next;
								free(search2);
								if(DEBUG) {
								printf("중간 엔트리 제거\n");
								}
							}
					}
					//이제 oldest에 값이 없어졌으므로 값을 넣는다
					oldestFrame->pid = i;
					oldestFrame->virtualPageNumber = vpn;						
					oldestFrame = oldestFrame->lruRight; //LRU최신화
						if(DEBUG) {
							printf("fault처리 완료\n");
						}
							}
				
				else {//search가 찾던 엔트리 찾음. hit
					if(DEBUG){
						printf("hit\n");
					}
					//printf("bbb\n");
					int find;
					find = search->frameNumber;
					procTable[i].numPageHit++;

					//물리 메모리에서 현재 접근한 프레임을 가장 최근에 쓴 걸로 바꾼다.
					physicalMemory[find].lruLeft->lruRight = physicalMemory[find].lruRight;
					physicalMemory[find].lruRight->lruLeft = physicalMemory[find].lruLeft;
					oldestFrame->lruLeft->lruRight = &physicalMemory[find];
					physicalMemory[find].lruLeft = oldestFrame->lruLeft;
					physicalMemory[find].lruRight = oldestFrame;
					oldestFrame->lruLeft = &physicalMemory[find];//lru최신화
				}
				if(DEBUG){
					printf("엔트리가 잇는 경우 끝났다\n");
				}
			}
//======================f=================================

			else {//아무런 값도 존재하지 않음->첫번째 위치에 새 엔트리를 생성후 oldest에 값을 넣는다
				if(DEBUG){
        			//printf("해시테이블에 아무것도 없을 때\n");
					printf("엔트리가 없음\n");
				}
				procTable[i].numIHTNULLAccess++;//엔트리가 존재하지 않았던 횟수 증가
				procTable[i].numPageFault++; //fault이므로 값을 증가시킴
				//struct invertedPageTableEntry* add = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry)*1);
				//add = hashTable[index].next;
				hashTable[index].next =(struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry)*1);
				hashTable[index].pid = i;//새 엔트리에 값 넣는중;
				hashTable[index].virtualPageNumber = vpn;
				hashTable[index].frameNumber = oldestFrame->number;
				//오른쪽 framenumber는 oldest의 fr amenumber
				hashTable[index].next = NULL;// 이제 oldestFrame에 값을 넣는다.
				if (oldestFrame->pid != -1) {//oldest에 이미 값이 존재하면 그 값제거후 다시 넣는다.
					//printf("올디스트에 값이 이미 존재");
					int oldPid, oldvpn,oldindex;
					oldPid = oldestFrame->pid; //예전 값 저장
					oldvpn = oldestFrame->virtualPageNumber;
					oldindex = (oldvpn + oldPid) %nFrame;
					//oldest의 예전 값을 지닌 리스트를 찾는다.
					struct invertedPageTableEntry* search3;
 					search3 = hashTable[oldindex].next;
					struct invertedPageTableEntry* before2;
					before2 = NULL;
					if(DEBUG) {
						printf("while문 전\n");
					}
					while (1) {
						if ((search3->virtualPageNumber == oldvpn) && (search3->pid == oldPid)) break; //리스트를 찾으면 나옴
						before2 = search3;
						search3 = search3->next;
					}
						if(DEBUG) {
							printf("while문 나옴\n");
						} //로직 다시짜기
							if((search3->next == NULL) && (before2->next = NULL)) {//유일한 엔트리
								hashTable[oldindex].next =NULL;
								free(search3);
								if(DEBUG) {
							printf("유일한 엔트리 제거2\n");}
							}//뒤에 무언가가 더 있으면
							else if(search3->next ==NULL && before2->next !=NULL) {
								before2->next = NULL;
								free(search3);
								if(DEBUG) {
								printf("마지막 엔트리 제거2\n");
								}
							}		
							else if (before2->next == NULL && search3->next !=NULL) { //가장 마지막에 존재하므로 그 전 리스트를 종점으로 하고 삭제
								hashTable[oldindex].next = search3->next;
								free(search3);
								if(DEBUG) {
							printf("처음 엔트리 제거2\n");}
							}
							else {
								before2->next = search3->next;
								free(search3);
								if(DEBUG) {
								printf("중간 엔트리 제거\n");
								}
							}
					}
					//이제 oldest에 값이 없어졌으므로 값을 넣는다
					oldestFrame->pid = i;
					oldestFrame->virtualPageNumber = vpn;						
					oldestFrame = oldestFrame->lruRight; //LRU최신화
						if(DEBUG) {
							printf("아무런 엔트리도 없을 떄 처리 완료\n");
						}
							}
				




					//if(search3 ==NULL) {//말이 안..돼..
					//printf("말이 안됨.. \n");
					//}

					//else{
					//printf("loop나옴\n");
						/*if (before2 == NULL) {
							if(search3->next != NULL) {
								hashTable[oldindex].next  = search3->next;
							free(search3); } 
                            else {
                                hashTable[oldindex].next = NULL;
                            free(search3);}
                        }
							//hashTable[oldindex].next = NULL;
							//가장 첫번째 리스트에 존재하므로 그것만 삭제
						else if(search3->next == NULL) { //가장 마지막에 존재하므로 그 전 리스트를 종점으로 하고 삭제
							free(search3);
							before2->next = NULL;
						}
                
						else {//찾은 엔트리가 중간에 존재할 경우
							before2->next = search3->next;
							free(search3);
						}
						//원래 oldestFrame에 존재했던 값을 해시테이블에서 제거함. 이제  old에 새 값을 넣어준다LRU최신화 작업을 한다
						//LRU최신화
						
					//printf("경우의 수 다 나옴\n");
                        
						oldestFrame->pid = i;
						oldestFrame->virtualPageNumber = vpn;
						oldestFrame = oldestFrame->lruRight;
						if(DEBUG){
						printf("엔트리도 없고 올디스트는 잇어서 다 완성시킴\n");
						}		
					}
				
				//printf("올디스트에 값이 존재 안함\n");//oldest에 값이 존재하지 않을 경우 바로 현재 접근하는 값을 넣고 LRU 최신화를 한다
                else{ 
					oldestFrame->pid = i;
					oldestFrame->virtualPageNumber = vpn;
					oldestFrame = oldestFrame->lruRight;
				}
                if(DEBUG) {
                printf("엔트리가 비엇을 경우 끝낫당\n");
			            } */
                }
              if(DEBUG) {
			printf("for문 끝낫습니당\n");
		}
        physicalAddr = (vpn << 30) + offset;
	printf("IHT procID %d traceNumber %d virtual addr %x pysical addr %x\n", i, procTable[i].ntraces, virtualaddr, physicalAddr);
		    }
if(DEBUG){
	printf("while문 마지막\n");
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
		//procTable[i].tracefp = fopen(argv[i+3],"r");
		printf("process %d opening %s \n", i, argv[i + 3]);
	}
	printf(" Num of Frames %d Physical Memory Size %ld bytes \n", nFrame, (1L << phyMemSizeBits));
	printf("============================================================= \n");

	//multiLevelTable(procTable, physicalMemory, secondLevelBits);
	//secondLevelVMSim(procTable, physicalMemory);
	//initProcTable(procTable, numProcess, argv); //프로세스테이블 초기화'
	//initPhyMem(physicalMemory, nFrame);//프로세스 테이블과 물리 메모리 만들기
	IPHT(procTable,physicalMemory);
	invertedPageVMSim(procTable, physicalMemory, nFrame);
}

		

	
		

