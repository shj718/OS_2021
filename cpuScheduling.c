// OS
// CPU Schedule Simulator Homework
// Student Number : B911088
// Name : 서혜준
// Year : 2021
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>

#define SEED 10

// process states
#define S_IDLE 0
#define S_READY 1
#define S_BLOCKED 2
#define S_RUNNING 3
#define S_TERMINATE 4

int NPROC, NIOREQ, QUANTUM; // 총 process 개수, 총 I/O 요청 개수, 퀀텀

struct ioDoneEvent {
    int procid;
    int doneTime;
    int len;
    struct ioDoneEvent* prev;
    struct ioDoneEvent* next;
} ioDoneEventQueue, * ioDoneEvent;

struct process {
    int id;
    int len;                // for queue
    int targetServiceTime;
    int serviceTime;
    int startTime;
    int endTime;
    char state;
    int priority;
    int saveReg0, saveReg1;
    struct process* prev;
    struct process* next;
} *procTable;

struct process idleProc;
struct process readyQueue;
struct process blockedQueue;
struct process* runningProc;
struct process* readyQueuePointer;
struct process* readyQueueReturn;

int cpuReg0, cpuReg1;
int currentTime = 0;
int* procIntArrTime, * procServTime, * ioReqIntArrTime, * ioServTime;

void compute() {
    // DO NOT CHANGE THIS FUNCTION
    cpuReg0 = cpuReg0 + runningProc->id;
    cpuReg1 = cpuReg1 + runningProc->id;
    //printf("In computer proc %d cpuReg0 %d\n",runningProc->id,cpuReg0);
}

void initProcTable() {
    int i;
    for (i = 0; i < NPROC; i++) {
        procTable[i].id = i;
        procTable[i].len = 0;
        procTable[i].targetServiceTime = procServTime[i];
        procTable[i].serviceTime = 0;
        procTable[i].startTime = 0;
        procTable[i].endTime = 0;
        procTable[i].state = S_IDLE;
        procTable[i].priority = 0;
        procTable[i].saveReg0 = 0;
        procTable[i].saveReg1 = 0;
        procTable[i].prev = NULL;
        procTable[i].next = NULL;
    }
}

void initIdleProc() {
    // idleProc을 초기화.
    idleProc.id = -1;
    idleProc.len = 0;
    idleProc.targetServiceTime = INT_MAX; // idleProc은 terminate되면 안되므로 serviceTime과 targetServiceTime이 항상 달라야함
    idleProc.serviceTime = 0;
    idleProc.startTime = 0;
    idleProc.endTime = 0;
    idleProc.state = S_IDLE;
    idleProc.priority = 0;
    idleProc.saveReg0 = 0;
    idleProc.saveReg1 = 0;
    idleProc.prev = NULL;
    idleProc.next = NULL;
}

int isReadyQueueEmpty() {
    if ((readyQueue.prev == &readyQueue) && (readyQueue.next == &readyQueue))
        return 1;
    else
        return 0;
}

void findFromReadyQueue(int index) {
    if (index <= readyQueue.len / 2) {
        readyQueuePointer = readyQueue.next;
        while (index > 0) {
            readyQueuePointer = readyQueuePointer->next;
            index--;
        }
    }
    else {
        int countFromBack;
        countFromBack = readyQueue.len - index - 1;
        readyQueuePointer = readyQueue.prev;
        while (countFromBack > 0) {
            readyQueuePointer = readyQueuePointer->prev;
            countFromBack--;
        }
    }
}

void removeFromReadyQueue() {
    readyQueuePointer->prev->next = readyQueuePointer->next;
    readyQueuePointer->next->prev = readyQueuePointer->prev;
    readyQueue.len--;
}

void procExecSim(struct process* (*scheduler)()) {
    int numWhileLoop = 0;
    int pid, qTime = 0, cpuUseTime = 0, nproc = 0, termProc = 0, nioreq = 0; // nproc은 현재까지 생성된 process의 개수
    char schedule = 0, nextState = S_IDLE; // schedule은 5개의 이벤트 중 하나라도 발생했는지를 나타냄.
    int nextForkTime, nextIOReqTime;
    int ioDoneEventQueueCnt, blockedQueueCnt; // ioDoneEvent큐와 blocked큐의 정렬 순서를 일치시키기 위한 변수.
    struct ioDoneEvent* ioDoneEventQueuePointer; // ioDoneEventQueue의 첫번째 event를 가리키게 초기화.
    struct process* blockedQueuePointer; // blockedQueue의 첫번째 process를 가리키게 초기화.

    nextForkTime = procIntArrTime[nproc]; // 처음에는 nextForkTime을 procIntArrTime[0]으로 초기화.
    nextIOReqTime = ioReqIntArrTime[nioreq]; // 처음에는 nextIOReqTime을 ioReqIntArrTime[0]으로 초기화.

    initIdleProc();
    runningProc = &idleProc; // 맨 처음에는 runningProc을 idleProc으로 초기화.

    while (1) {
        if (termProc == NPROC)
            return; // 시뮬레이션 종료.
        currentTime++;
        qTime++;
        runningProc->serviceTime++;
        if (runningProc != &idleProc) cpuUseTime++;
        schedule = 0; // 매 사이클마다 0으로 초기화.
        cpuReg0 = runningProc->saveReg0; // cpuReg값 복원.
        cpuReg1 = runningProc->saveReg1;

        // MUST CALL compute() Inside While loop
        compute();

        runningProc->saveReg0 = cpuReg0; // compute()한 결과 저장.
        runningProc->saveReg1 = cpuReg1;

        numWhileLoop++;
        /* printf("%d processing Proc %d servT %d targetServT %d ", numWhileLoop, runningProc->id, runningProc->serviceTime, runningProc->targetServiceTime);
        printf("nproc %d cpuUseTime %d qTime %d ", nproc, cpuUseTime, qTime);
        printf("proc %d ioDoneTime %d ioDoneEvent Length %d \n", ioDoneEventQueue.next->procid, ioDoneEventQueue.next->doneTime, ioDoneEventQueue.len); */
        if (currentTime == nextForkTime) { /* CASE 2 : a new process created */
            procTable[nproc].startTime = currentTime; // procTable의 startTime에 현재시간 기록.
            procTable[nproc].state = S_READY; // 새로운 process의 상태를 READY 상태로 전환.
            procTable[nproc].prev = readyQueue.prev; // 현재 line ~ 밑 4줄 : ready큐에 삽입, ready큐의 len을 1만큼 증가시킴
            procTable[nproc].next = &readyQueue;
            procTable[nproc].prev->next = &procTable[nproc];
            procTable[nproc].next->prev = &procTable[nproc];
            readyQueue.len++;

            /* printf("process %d targetST %d servT %d added to ReadyQueue, ", procTable[nproc].id, procTable[nproc].targetServiceTime, procTable[nproc].serviceTime); */

            nproc++; // nproc(현재까지 생성된 process의 개수)를 1만큼 증가시킴
            if (nproc != NPROC) nextForkTime = currentTime + procIntArrTime[nproc]; // 다음 process가 생성되어야할 시간 재설정.
            else nextForkTime = INT_MAX;

            /* printf("NextForkTime %d \n", nextForkTime); */

            schedule = 1;
            nextState = S_READY;
        }
        if (qTime == QUANTUM) { /* CASE 1 : The quantum expires */
            if (runningProc != &idleProc) {
                runningProc->priority--; // Simple Feedback 스케줄링을 위한 priority 감소.
                schedule = 1;
                nextState = S_READY;
            }

            /* printf("process %d servT %d targetServT %d Quantum Expires\n", runningProc->id, runningProc->serviceTime, runningProc->targetServiceTime); */
        }
        while (ioDoneEventQueue.next->doneTime == currentTime) { /* CASE 3 : IO Done Event */
            pid = ioDoneEventQueue.next->procid;

            /* printf("IO Done Event for pid %d \n", pid); */

            if (procTable[pid].state == S_BLOCKED) {
                /* blockedQueue에서 해당 process 삭제. */
                blockedQueuePointer = blockedQueue.next; // 포인터 초기화.
                while (blockedQueuePointer->id != pid) blockedQueuePointer = blockedQueuePointer->next;
                blockedQueuePointer->prev->next = blockedQueuePointer->next;
                blockedQueuePointer->next->prev = blockedQueuePointer->prev;
                blockedQueue.len--;
                procTable[pid].state = S_READY; // blocked 되었던 process의 상태를 ready로.
                /* readyQueue에 해당 process 삽입. */
                procTable[pid].prev = readyQueue.prev;
                procTable[pid].next = &readyQueue;
                procTable[pid].prev->next = &procTable[pid];
                procTable[pid].next->prev = &procTable[pid];
                readyQueue.len++;

                /* printf("**IO Done Move proc %d Blocked to Ready\nBlockedQueue Length %d \n", pid, blockedQueue.len); */
            }
            /* else if (procTable[pid].state == S_TERMINATE) {
                printf("**IO Done Move proc %d Blocked to Terminate\nBlockedQueue Length %d \n", pid, blockedQueue.len);
            } */

            /* ioDoneEventQueue에서 해당 Event 삭제 */
            ioDoneEventQueue.next = ioDoneEventQueue.next->next;
            ioDoneEventQueue.next->prev = &ioDoneEventQueue;
            ioDoneEventQueue.len--;
            schedule = 1;
            nextState = S_READY;
        }
        if (cpuUseTime == nextIOReqTime) { /* CASE 5: reqest IO operations (only when the process does not terminate) */
            if (runningProc != &idleProc) {
                schedule = 1;
                nextState = S_BLOCKED;

                /* printf("process %d servTime %d targetST %d IO Req Event\n", runningProc->id, runningProc->serviceTime, runningProc->targetServiceTime); */
            }
        }
        if (runningProc->serviceTime == runningProc->targetServiceTime) { /* CASE 4 : the process job done and terminates */
            schedule = 1;
            nextState = S_TERMINATE;

            /* printf("process %d targetST %d servTime %d terminates\n", runningProc->id, runningProc->targetServiceTime, runningProc->serviceTime); */
        }

        /* runningProc의 state 변경. (단, runningProc이 idleProc이 아니어야함) */
        if (schedule == 1 && runningProc != &idleProc) {
            if (nextState == S_TERMINATE) { /* 종료 CASE */
                if (cpuUseTime == nextIOReqTime) { /* io요청과 종료가 동시에 발생한 경우. */
                    /* ioDoneEvent 발생. */
                    ioDoneEvent[nioreq].procid = runningProc->id; // io 요청한 process의 id를 ioDoneEvent에 저장.
                    ioDoneEvent[nioreq].doneTime = currentTime + ioServTime[nioreq]; // io 요청의 doneTime 계산.

                    ioDoneEventQueuePointer = ioDoneEventQueue.next; // 포인터 초기화.
                    ioDoneEventQueueCnt = 0; // Cnt 초기화.
                    /* ioDoneEventQueue에 삽입, 큐 길이 1만큼 증가. */
                    while (ioDoneEventQueuePointer->doneTime <= ioDoneEvent[nioreq].doneTime) {
                        ioDoneEventQueuePointer = ioDoneEventQueuePointer->next;
                        ioDoneEventQueueCnt++;
                    }
                    ioDoneEventQueuePointer->prev->next = &ioDoneEvent[nioreq];
                    ioDoneEvent[nioreq].prev = ioDoneEventQueuePointer->prev;
                    ioDoneEventQueuePointer->prev = &ioDoneEvent[nioreq];
                    ioDoneEvent[nioreq].next = ioDoneEventQueuePointer;
                    ioDoneEventQueue.len++; // ioDoneEventQueue에 삽입 완료.

                    /* printf("Process %d io Request : Done Event %d th added with doneTime = %d \n", runningProc->id, nioreq, ioDoneEvent[nioreq].doneTime); */

                    nioreq++;
                    if (nioreq != NIOREQ) nextIOReqTime = cpuUseTime + ioReqIntArrTime[nioreq];
                    else nextIOReqTime = INT_MAX;
                    /* printf("terminate and ioreq at the same time! %d th nextIOReqTime %d \n", nioreq, nextIOReqTime); */
                }
                runningProc->endTime = currentTime; // endTime에 현재시간을 기록.
                runningProc->state = nextState; // runningProc의 상태 변경.
                termProc++; // 종료된 process의 개수 1만큼 증가시킴

                /* printf("**%d-th proc terminated at %d : Process %d \n", termProc, numWhileLoop, runningProc->id);
                printf("ReadyQueue Length %d \nBlockedQueue Length %d \n", readyQueue.len, blockedQueue.len); */
            }
            else if (nextState == S_BLOCKED) { /* io 요청 CASE */
                /* ioDoneEvent 생성 및 runningProc을 ready큐에 삽입. */
                if (qTime != QUANTUM) runningProc->priority++; // Simple Feedback 스케줄링을 위한 priority 증가.
                runningProc->state = nextState; // runningProc의 상태 변경.
                /* ioDoneEvent 발생. */
                ioDoneEvent[nioreq].procid = runningProc->id; // io 요청한 process의 id를 ioDoneEvent에 저장.
                ioDoneEvent[nioreq].doneTime = currentTime + ioServTime[nioreq]; // io 요청의 doneTime 계산.

                ioDoneEventQueuePointer = ioDoneEventQueue.next; // 포인터 초기화.
                ioDoneEventQueueCnt = 0; // Cnt 초기화.
                /* ioDoneEventQueue에 삽입, 큐 길이 1만큼 증가. */
                while (ioDoneEventQueuePointer->doneTime <= ioDoneEvent[nioreq].doneTime) {
                    ioDoneEventQueuePointer = ioDoneEventQueuePointer->next;
                    ioDoneEventQueueCnt++;
                }
                ioDoneEventQueuePointer->prev->next = &ioDoneEvent[nioreq];
                ioDoneEvent[nioreq].prev = ioDoneEventQueuePointer->prev;
                ioDoneEventQueuePointer->prev = &ioDoneEvent[nioreq];
                ioDoneEvent[nioreq].next = ioDoneEventQueuePointer;
                ioDoneEventQueue.len++; // ioDoneEventQueue에 삽입 완료.

                blockedQueuePointer = blockedQueue.next; // 포인터 초기화.
                blockedQueueCnt = 0; // Cnt 초기화.
                /* blocked큐에 삽입, 큐 길이 1만큼 증가. */
                while (blockedQueueCnt != ioDoneEventQueueCnt) {
                    blockedQueuePointer = blockedQueuePointer->next;
                    blockedQueueCnt++;
                }
                blockedQueuePointer->prev->next = runningProc;
                runningProc->prev = blockedQueuePointer->prev;
                blockedQueuePointer->prev = runningProc;
                runningProc->next = blockedQueuePointer;
                blockedQueue.len++; // blocked큐에 삽입 완료.

                /* printf("Process %d io Request : Done Event %d th added with doneTime = %d \n", runningProc->id, nioreq, ioDoneEvent[nioreq].doneTime); */
                
                nioreq++;
                if (nioreq != NIOREQ) nextIOReqTime = cpuUseTime + ioReqIntArrTime[nioreq];
                else nextIOReqTime = INT_MAX;

                /* printf("%d th nextIOReqTime %d \n", nioreq, nextIOReqTime);
                printf("**Proc %d move to BlockedQueue\n", runningProc->id);
                printf("BlockedQueue Length %d \nReadyQueue Length %d \n", blockedQueue.len, readyQueue.len); */
            }
            else { /* 프로세스의 생성 || Quantum 만료 || io 처리가 끝남 */
                runningProc->state = nextState; // runningProc의 상태 변경.
                runningProc->prev = readyQueue.prev; // ready큐에 삽입.
                runningProc->next = &readyQueue;
                runningProc->prev->next = runningProc;
                runningProc->next->prev = runningProc;
                readyQueue.len++;
               
                /* printf("**Proc %d move to ReadyQueue\n", runningProc->id);
                printf("ReadyQueue Length %d \nBlockedQueue Length %d \n", readyQueue.len, blockedQueue.len); */
            }
        }
        if (schedule == 1) {
            runningProc = scheduler();
            runningProc->state = S_RUNNING;
            qTime = 0; //qTime 초기화.
            /* printf("Scheduler selects process %d \n", runningProc->id);
            printf("ReadyQueue Length %d \n", readyQueue.len); */
        }

    } // while loop
}

//RR,SJF(Modified),SRTN,Guaranteed Scheduling(modified),Simple Feed Back Scheduling
struct process* RRschedule() {
    if (isReadyQueueEmpty())
        return &idleProc; // ready큐가 비었다면, idleProc 리턴.
    readyQueueReturn = readyQueue.next; // ready큐의 첫 process 임시 저장.
    readyQueue.next = readyQueue.next->next; // ready큐에서 첫 process 삭제.
    readyQueue.next->prev = &readyQueue;
    readyQueue.len--; // ready큐 길이 1만큼 감소.
    return readyQueueReturn;
}
struct process* SJFschedule() {
    if (isReadyQueueEmpty())
        return &idleProc; // ready큐가 비었다면, idleProc 리턴.
    int shortestJobTime, index = 0, count = 0;
    readyQueuePointer = readyQueue.next; // 포인터 초기화.
    shortestJobTime = readyQueuePointer->targetServiceTime; // shortestJob시간 초기화.
    while (readyQueuePointer != &readyQueue) { /* shortestJob이 여러개면 FCFS. */
        if (readyQueuePointer->targetServiceTime < shortestJobTime) {
            shortestJobTime = readyQueuePointer->targetServiceTime;
            index = count;
        }
        readyQueuePointer = readyQueuePointer->next;
        count++;
    }
    findFromReadyQueue(index);
    readyQueueReturn = readyQueuePointer; // 리턴할 process 임시 저장.
    removeFromReadyQueue();
    return readyQueueReturn;
}
struct process* SRTNschedule() {
    if (isReadyQueueEmpty())
        return &idleProc; // ready큐가 비었다면, idleProc 리턴.
    int shortestRemainingTime, index = 0, count = 0;
    readyQueuePointer = readyQueue.next; // 포인터 초기화.
    shortestRemainingTime = readyQueuePointer->targetServiceTime - readyQueuePointer->serviceTime; // 최솟값 초기화.
    while (readyQueuePointer != &readyQueue) {
        if (readyQueuePointer->targetServiceTime - readyQueuePointer->serviceTime < shortestRemainingTime) {
            shortestRemainingTime = readyQueuePointer->targetServiceTime - readyQueuePointer->serviceTime;
            index = count;
        }
        readyQueuePointer = readyQueuePointer->next;
        count++;
    }
    findFromReadyQueue(index);
    readyQueueReturn = readyQueuePointer; // 리턴할 process 임시 저장.
    removeFromReadyQueue();
    return readyQueueReturn;
}
struct process* GSschedule() {
    if (isReadyQueueEmpty())
        return &idleProc; // ready큐가 비었다면, idleProc 리턴.
    double lowestRatio;
    int index = 0, count = 0;
    readyQueuePointer = readyQueue.next; // 포인터 초기화.
    lowestRatio = (double)readyQueuePointer->serviceTime / (double)readyQueuePointer->targetServiceTime; // 최소 ratio 초기화.
    while (readyQueuePointer != &readyQueue) {
        if ((double)readyQueuePointer->serviceTime / (double)readyQueuePointer->targetServiceTime < lowestRatio) {
            lowestRatio = (double)readyQueuePointer->serviceTime / (double)readyQueuePointer->targetServiceTime;
            index = count;
        }
        readyQueuePointer = readyQueuePointer->next;
        count++;
    }
    findFromReadyQueue(index);
    readyQueueReturn = readyQueuePointer; // 리턴할 process 임시 저장.
    removeFromReadyQueue();
    return readyQueueReturn;
}
struct process* SFSschedule() {
    if (isReadyQueueEmpty())
        return &idleProc; // ready큐가 비었다면, idleProc 리턴.
    int highestPriority, index = 0, count = 0;
    readyQueuePointer = readyQueue.next; // 포인터 초기화.
    highestPriority = readyQueuePointer->priority; // 가장 높은 priority 초기화.
    while (readyQueuePointer != &readyQueue) {
        if (readyQueuePointer->priority > highestPriority) {
            highestPriority = readyQueuePointer->priority;
            index = count;
        }
        readyQueuePointer = readyQueuePointer->next;
        count++;
    }
    findFromReadyQueue(index);
    readyQueueReturn = readyQueuePointer; // 리턴할 process 임시 저장.
    removeFromReadyQueue();
    return readyQueueReturn;
}

void printResult() {
    // DO NOT CHANGE THIS FUNCTION
    int i;
    long totalProcIntArrTime = 0, totalProcServTime = 0, totalIOReqIntArrTime = 0, totalIOServTime = 0;
    long totalWallTime = 0, totalRegValue = 0;
    for (i = 0; i < NPROC; i++) {
        totalWallTime += procTable[i].endTime - procTable[i].startTime;
        /*
        printf("proc %d serviceTime %d targetServiceTime %d saveReg0 %d\n",
                i,procTable[i].serviceTime,procTable[i].targetServiceTime, procTable[i].saveReg0);
        */
        totalRegValue += procTable[i].saveReg0 + procTable[i].saveReg1;
        /* printf("reg0 %d reg1 %d totalRegValue %d\n",procTable[i].saveReg0,procTable[i].saveReg1,totalRegValue);*/
    }
    for (i = 0; i < NPROC; i++) {
        totalProcIntArrTime += procIntArrTime[i];
        totalProcServTime += procServTime[i];
    }
    for (i = 0; i < NIOREQ; i++) {
        totalIOReqIntArrTime += ioReqIntArrTime[i];
        totalIOServTime += ioServTime[i];
    }

    printf("Avg Proc Inter Arrival Time : %g \tAverage Proc Service Time : %g\n", (float)totalProcIntArrTime / NPROC, (float)totalProcServTime / NPROC);
    printf("Avg IOReq Inter Arrival Time : %g \tAverage IOReq Service Time : %g\n", (float)totalIOReqIntArrTime / NIOREQ, (float)totalIOServTime / NIOREQ);
    printf("%d Process processed with %d IO requests\n", NPROC, NIOREQ);
    printf("Average Wall Clock Service Time : %g \tAverage Two Register Sum Value %g\n", (float)totalWallTime / NPROC, (float)totalRegValue / NPROC);

}

int main(int argc, char* argv[]) {
    // DO NOT CHANGE THIS FUNCTION
    int i;
    int totalProcServTime = 0, ioReqAvgIntArrTime;
    int SCHEDULING_METHOD, MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME, MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME;

    if (argc < 12) {
        printf("%s: SCHEDULING_METHOD NPROC NIOREQ QUANTUM MIN_INT_ARRTIME MAX_INT_ARRTIME MIN_SERVTIME MAX_SERVTIME MIN_IO_SERVTIME MAX_IO_SERVTIME MIN_IOREQ_INT_ARRTIME\n", argv[0]);
        exit(1);
    }

    SCHEDULING_METHOD = atoi(argv[1]);
    NPROC = atoi(argv[2]);
    NIOREQ = atoi(argv[3]);
    QUANTUM = atoi(argv[4]);
    MIN_INT_ARRTIME = atoi(argv[5]);
    MAX_INT_ARRTIME = atoi(argv[6]);
    MIN_SERVTIME = atoi(argv[7]);
    MAX_SERVTIME = atoi(argv[8]);
    MIN_IO_SERVTIME = atoi(argv[9]);
    MAX_IO_SERVTIME = atoi(argv[10]);
    MIN_IOREQ_INT_ARRTIME = atoi(argv[11]);

    printf("SIMULATION PARAMETERS : SCHEDULING_METHOD %d NPROC %d NIOREQ %d QUANTUM %d \n", SCHEDULING_METHOD, NPROC, NIOREQ, QUANTUM);
    printf("MIN_INT_ARRTIME %d MAX_INT_ARRTIME %d MIN_SERVTIME %d MAX_SERVTIME %d\n", MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME);
    printf("MIN_IO_SERVTIME %d MAX_IO_SERVTIME %d MIN_IOREQ_INT_ARRTIME %d\n", MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME);

    srandom(SEED);

    // allocate array structures
    procTable = (struct process*) malloc(sizeof(struct process) * NPROC);
    ioDoneEvent = (struct ioDoneEvent*) malloc(sizeof(struct ioDoneEvent) * NIOREQ);
    procIntArrTime = (int*)malloc(sizeof(int) * NPROC);
    procServTime = (int*)malloc(sizeof(int) * NPROC);
    ioReqIntArrTime = (int*)malloc(sizeof(int) * NIOREQ);
    ioServTime = (int*)malloc(sizeof(int) * NIOREQ);

    // initialize queues
    readyQueue.next = readyQueue.prev = &readyQueue;
    blockedQueue.next = blockedQueue.prev = &blockedQueue;
    ioDoneEventQueue.next = ioDoneEventQueue.prev = &ioDoneEventQueue;
    ioDoneEventQueue.doneTime = INT_MAX;
    ioDoneEventQueue.procid = -1;
    ioDoneEventQueue.len = readyQueue.len = blockedQueue.len = 0;

    // generate process interarrival times
    for (i = 0; i < NPROC; i++) {
        procIntArrTime[i] = random() % (MAX_INT_ARRTIME - MIN_INT_ARRTIME + 1) + MIN_INT_ARRTIME;
    }

    // assign service time for each process
    for (i = 0; i < NPROC; i++) {
        procServTime[i] = random() % (MAX_SERVTIME - MIN_SERVTIME + 1) + MIN_SERVTIME;
        totalProcServTime += procServTime[i];
    }

    ioReqAvgIntArrTime = totalProcServTime / (NIOREQ + 1);
    assert(ioReqAvgIntArrTime > MIN_IOREQ_INT_ARRTIME);

    // generate io request interarrival time
    for (i = 0; i < NIOREQ; i++) {
        ioReqIntArrTime[i] = random() % ((ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME) * 2 + 1) + MIN_IOREQ_INT_ARRTIME;
    }

    // generate io request service time
    for (i = 0; i < NIOREQ; i++) {
        ioServTime[i] = random() % (MAX_IO_SERVTIME - MIN_IO_SERVTIME + 1) + MIN_IO_SERVTIME;
    }

#ifdef DEBUG
    // printing process interarrival time and service time
    printf("Process Interarrival Time :\n");
    for (i = 0; i < NPROC; i++) {
        printf("%d ", procIntArrTime[i]);
    }
    printf("\n");
    printf("Process Target Service Time :\n");
    for (i = 0; i < NPROC; i++) {
        printf("%d ", procTable[i].targetServiceTime);
    }
    printf("\n");
#endif

    // printing io request interarrival time and io request service time
    printf("IO Req Average InterArrival Time %d\n", ioReqAvgIntArrTime);
    printf("IO Req InterArrival Time range : %d ~ %d\n", MIN_IOREQ_INT_ARRTIME,
        (ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME) * 2 + MIN_IOREQ_INT_ARRTIME);


#ifdef DEBUG
    printf("IO Req Interarrival Time :\n");
    for (i = 0; i < NIOREQ; i++) {
        printf("%d ", ioReqIntArrTime[i]);
    }
    printf("\n");
    printf("IO Req Service Time :\n");
    for (i = 0; i < NIOREQ; i++) {
        printf("%d ", ioServTime[i]);
    }
    printf("\n");
#endif

    struct process* (*schFunc)();
    switch (SCHEDULING_METHOD) {
    case 1: schFunc = RRschedule; break;
    case 2: schFunc = SJFschedule; break;
    case 3: schFunc = SRTNschedule; break;
    case 4: schFunc = GSschedule; break;
    case 5: schFunc = SFSschedule; break;
    default: printf("ERROR : Unknown Scheduling Method\n"); exit(1);
    }
    initProcTable();
    procExecSim(schFunc);
    printResult();

}
