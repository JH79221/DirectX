/*
          [Start Game]
                |
                v
+ ---------------------------+
|   1. Process Input         | //키보드, 마우스 등 사용자의 개입 확인
+ ---------------------------+
                |
                v
+ ---------------------------+
|   2. Update Game State     | //캐릭터 이동, 충돌 계산, AI 로직 수행
+ ---------------------------+
                |
                v
+ ---------------------------+
|   3. Render Frame          | //계산된 데이터를 화면에 시각화
+ ---------------------------+
                |                                                                      
      [isRunning == true ? ] ------------> [1. Process Input 으로] //반복(Loop)                                                       
                |                                                                      
                v                                                                      
           [End Game] 
*/

#include <stdio.h>

// 게임의 모든 상태를 담는 바구니 (추상화)
typedef struct {
    int playerPos;
    int isRunning;
    char currentInput;
} GameContext;

// --- 1. 입력 단계 (Process Input) ---
// 정석: "무슨 일이 일어났는지"만 기록함.
void ProcessInput(GameContext* ctx) {
    printf("\n[A:왼쪽, D:오른쪽, Q:종료] 입력하세요: ");
    // 공백을 넣어 이전 버퍼를 비우고 입력을 받음
    scanf_s(" %c", &(ctx->currentInput));
}

// --- 2. 업데이트 단계 (Update) ---
// 정석: 입력된 정보를 바탕으로 "세상의 법칙"을 적용함.
void Update(GameContext* ctx) {
    // 종료 판정
    if (ctx->currentInput == 'q' || ctx->currentInput == 'Q') {
        ctx->isRunning = 0;
        return;
    }

    // 데이터 변화 (로직)
    if (ctx->currentInput == 'a' || ctx->currentInput == 'A') {
        ctx->playerPos--;
    }
    else if (ctx->currentInput == 'd' || ctx->currentInput == 'D') {
        ctx->playerPos++;
    }

    // 세상의 규칙 (Boundary Check)
    if (ctx->playerPos < 0) ctx->playerPos = 0;
    if (ctx->playerPos > 10) ctx->playerPos = 10;
}

// --- 3. 출력 단계 (Render) ---
// 정석: 현재 데이터 상태를 있는 그대로 "그리기"만 함.
void Render(GameContext* ctx) {
    // 화면 초기화 흉내 (실제 게임에서는 매 프레임 화면을 지움)
    printf("\n\n\n\n\n");

    printf("========== GAME SCREEN ==========\n");
    printf(" Player Position: %d\n", ctx->playerPos);
    printf(" [");
    for (int i = 0; i <= 10; i++) {
        if (i == ctx->playerPos) printf("P");
        else printf("_");
    }
    printf("]\n");
    printf("=================================\n");
}

int main() {
    // 초기화 (Initialization)
    GameContext game = { 5, 1, ' ' };

    printf("게임을 시작합니다. (정석 루프 패턴)\n");

    // [ 정석 게임 루프 ]
    while (game.isRunning) {
        // 1. 입력: 사용자가 무엇을 했는가?
        ProcessInput(&game);

        // 2. 업데이트: 그 결과 세상은 어떻게 변했는가?
        Update(&game);

        // 3. 출력: 변한 세상을 화면에 그려라!
        Render(&game);
    }

    printf("게임이 안전하게 종료되었습니다.\n");
    return 0;
}