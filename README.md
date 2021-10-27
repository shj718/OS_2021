# OS_2021

:star2: procExecSim 함수 구현의 핵심: 시뮬레이션중 발생할 수 있는 5가지 event들을 검사하는 if문들 안에서 nextState에 현재 running중인 process의 다음 상태를 저장한 후, 마지막에 running process의 상태를 바꾸기 + 여러가지 event들의 동시 발생을 고려하기

:heavy_exclamation_mark: 디버깅하면서 발견한 오류들 :heavy_exclamation_mark:
1) idle process는 퀀텀이 만료되지 않아야함.
2) GS 스케줄링에서 ratio 계산시 (double)로 명시적 형변환 필요.
3) 프로세스/IO요청이 NPROC/NIOREQ 만큼 다 만들어졌다면, nextForkTime/nextIOReqTime이 INT_MAX가 되도록 설정해야함.
4) 퀀텀 만료와 IO요청이 동시 발생시 퀀텀 만료에 의한 priority--; 만 수행.
5) 처음에 나는 종료와 IO요청이 동시 발생시 아예 ioDoneEvent를 만들지 않았는데, 만들어야 함.
