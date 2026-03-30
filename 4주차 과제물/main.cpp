#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h> // [NEW] printf 사용을 위한 헤더 추가

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// --- 전역 변수 (렌더링 자원들) ---
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11Buffer* g_pVBuffer = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11VertexShader* g_pVShader = nullptr;
ID3D11PixelShader* g_pPShader = nullptr;
ID3D11Buffer* g_pConstantBuffer = nullptr;

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

// --- 게임 상태 관리 (GameContext) ---
typedef struct {
    float playerX;
    float playerY;
    int isRunning;
} GameContext;

// 셰이더 코드
const char* shaderSource = R"(
cbuffer TransformBuffer : register(b0) {
    float2 offset;
    float2 padding;
};
struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos.x + offset.x, input.pos.y + offset.y, input.pos.z, 1.0f);
    output.col = input.col;
    return output;
}
float4 PS(PS_INPUT input) : SV_Target { return input.col; }
)";

// --- 1. 입력 단계 (Process Input) ---
// [MODIFIED] 프레임 방어를 위해 DeltaTime을 매개변수로 받아 이동 속도에 곱합니다.
void ProcessInput(GameContext* ctx, double deltaTime) {
    if (GetAsyncKeyState('Q') & 0x8000) ctx->isRunning = 0;

    // 1초에 1.0f 만큼 이동하도록 속도 설정 (기존의 고정 프레임 이동값 대체)
    float speed = 1.0f;

    if (GetAsyncKeyState('A') & 0x8000) ctx->playerX -= (float)(speed * deltaTime);
    if (GetAsyncKeyState('D') & 0x8000) ctx->playerX += (float)(speed * deltaTime);
    if (GetAsyncKeyState('W') & 0x8000) ctx->playerY += (float)(speed * deltaTime);
    if (GetAsyncKeyState('S') & 0x8000) ctx->playerY -= (float)(speed * deltaTime);
}

// --- 2. 업데이트 단계 (Update) ---
void Update(GameContext* ctx) {
    if (ctx->playerX < -0.5f) ctx->playerX = -0.5f;
    if (ctx->playerX > 0.5f) ctx->playerX = 0.5f;
    if (ctx->playerY < -0.5f) ctx->playerY = -0.5f;
    if (ctx->playerY > 0.5f) ctx->playerY = 0.5f;
}

// --- 3. 출력 단계 (Render) ---
void Render(GameContext* ctx) {
    float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
    D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0.0f, 1.0f };
    g_pImmediateContext->RSSetViewports(1, &vp);

    float cbData[4] = { ctx->playerX, ctx->playerY, 0.0f, 0.0f };
    g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, cbData, 0, 0);
    g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);

    g_pImmediateContext->IASetInputLayout(g_pInputLayout);
    UINT stride = sizeof(Vertex), offset = 0;
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVBuffer, &stride, &offset);
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_pImmediateContext->VSSetShader(g_pVShader, nullptr, 0);
    g_pImmediateContext->PSSetShader(g_pPShader, nullptr, 0);

    g_pImmediateContext->Draw(6, 0);

    // [MODIFIED] VSync 활성화 (Present(1, 0)). 
    // 픽셀 손실(Tearing)을 막고 모니터 주사율에 맞춰 부드럽게 출력합니다.
    g_pSwapChain->Present(1, 0);
}

// 윈도우 프로시저
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// --- Win32 메인 진입점 ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // [ 윈도우 생성 코드 ]
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"DX11Win32Class";
    RegisterClassEx(&wcex);

    HWND hWnd = CreateWindow(L"DX11Win32Class", L"DirectX 11 Game Loop - Hexagram",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return -1;
    ShowWindow(hWnd, nCmdShow);

    // [ DirectX 11 초기화 ]
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800;
    sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    // [ 셰이더 및 버퍼 생성 ]
    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);

    Vertex vertices[] = {
        {  0.0f,  0.5f, 0.5f,  1.0f, 0.0f, 1.0f, 1.0f },
        {  0.433f, -0.25f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
        { -0.433f, -0.25f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
        {  0.0f, -0.5f, 0.5f,  1.0f, 1.0f, 0.0f, 1.0f },
        { -0.433f, 0.25f, 0.5f,  0.0f, 1.0f, 0.0f, 1.0f },
        {  0.433f, 0.25f, 0.5f,  1.0f, 1.0f, 0.0f, 1.0f },
    };
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &g_pVBuffer);

    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = 16;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    g_pd3dDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer);

    GameContext game = { 0.0f, 0.0f, 1 };
    MSG msg = { 0 };

    // [NEW] 고해상도 타이머(QPC) 변수 초기화
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    LARGE_INTEGER prevTime, currTime;
    QueryPerformanceCounter(&prevTime);

    double deltaTime = 0.0;
    double elapsedTime = 0.0; // 1초를 세기 위한 누적 시간
    int frameCount = 0;       // 1초 동안의 렌더링 횟수

    // --- 메인 게임 루프 ---
    while (game.isRunning) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // 1. DeltaTime 계산 (게임 루프의 시작)
            QueryPerformanceCounter(&currTime);
            deltaTime = (double)(currTime.QuadPart - prevTime.QuadPart) / frequency.QuadPart;
            prevTime = currTime;

            // 누적 시간 및 프레임 카운트 증가
            elapsedTime += deltaTime;
            frameCount++;

            // 2. 시간 손실 없는 FPS 및 DeltaTime 출력 (정확히 1초마다 1번만 콘솔 출력)
            if (elapsedTime >= 1.0) {
                printf("DeltaTime: %.6f초 | FPS: %d\n", deltaTime, frameCount);
                elapsedTime -= 1.0; // 누적 시간 초기화
                frameCount = 0;     // 프레임 카운트 초기화
            }

            // 3. 게임 로직 및 렌더링
            ProcessInput(&game, deltaTime);
            Update(&game);
            Render(&game);
        }
    }

    // 자원 해제
    if (g_pConstantBuffer) g_pConstantBuffer->Release();
    if (g_pVBuffer) g_pVBuffer->Release();
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pVShader) g_pVShader->Release();
    if (g_pPShader) g_pPShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}