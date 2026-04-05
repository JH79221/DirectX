#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <vector>
#include <memory>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// --- 전역 변수 (렌더링 자원들) ---
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11VertexShader* g_pVShader = nullptr;
ID3D11PixelShader* g_pPShader = nullptr;
ID3D11Buffer* g_pConstantBuffer = nullptr;

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

// 셰이더 코드 (오프셋 이동 포함)
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
class GameObject;

// 1. 추상 클래스 Component
class Component {
protected:
    GameObject* owner; // 자신이 부착된 게임 오브젝트를 가리킴
public:
    Component(GameObject* owner) : owner(owner) {}
    virtual ~Component() = default;

    virtual void Update(float dt) {}
    virtual void Render() {}
};

// 2. GameObject 클래스
class GameObject {
public:
    float x, y;
    std::vector<std::unique_ptr<Component>> components;

    GameObject(float startX, float startY) : x(startX), y(startY) {}

    // 컴포넌트를 추가하는 템플릿 함수
    template <typename T, typename... Args>
    void AddComponent(Args&&... args) {
        components.push_back(std::make_unique<T>(this, std::forward<Args>(args)...));
    }

    void Update(float dt) {
        for (auto& c : components) {
            c->Update(dt);
        }
        // 화면 밖으로 나가지 않도록 클램핑
        if (x < -1.0f) x = -1.0f; if (x > 1.0f) x = 1.0f;
        if (y < -1.0f) y = -1.0f; if (y > 1.0f) y = 1.0f;
    }

    void Render() {
        for (auto& c : components) {
            c->Render();
        }
    }
};

// 3. Renderer 컴포넌트 (삼각형 그리기)
class RendererComponent : public Component {
private:
    ID3D11Buffer* pVBuffer = nullptr;

public:
    RendererComponent(GameObject* owner, float r, float g, float b) : Component(owner) {
        // 객체마다 지정된 색상으로 버텍스 버퍼 생성
        Vertex vertices[] = {
            {  0.0f,  0.15f, 0.5f,  r, g, b, 1.0f },
            {  0.15f, -0.15f, 0.5f, r, g, b, 1.0f },
            { -0.15f, -0.15f, 0.5f, r, g, b, 1.0f },
        };
        D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
        D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
        g_pd3dDevice->CreateBuffer(&bd, &initData, &pVBuffer);
    }

    ~RendererComponent() {
        if (pVBuffer) pVBuffer->Release();
    }

    void Render() override {
        // 부모 GameObject의 위치 정보를 Constant Buffer에 업데이트
        float cbData[4] = { owner->x, owner->y, 0.0f, 0.0f };
        g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, cbData, 0, 0);

        // 버텍스 버퍼 설정 후 Draw
        UINT stride = sizeof(Vertex), offset = 0;
        g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);
        g_pImmediateContext->Draw(3, 0);
    }
};

// 4. Player Controller 컴포넌트 (입력 처리 및 이동)
class PlayerControllerComponent : public Component {
private:
    int upKey, downKey, leftKey, rightKey;
    float speed = 1.5f; // 이동 속도 (Velocity)

public:
    PlayerControllerComponent(GameObject* owner, int up, int down, int left, int right)
        : Component(owner), upKey(up), downKey(down), leftKey(left), rightKey(right) {
    }

    void Update(float dt) override {
        // 공식: Position = Position + (Velocity * DeltaTime)
        if (GetAsyncKeyState(leftKey) & 0x8000)  owner->x -= speed * dt;
        if (GetAsyncKeyState(rightKey) & 0x8000) owner->x += speed * dt;
        if (GetAsyncKeyState(upKey) & 0x8000)    owner->y += speed * dt;
        if (GetAsyncKeyState(downKey) & 0x8000)  owner->y -= speed * dt;
    }
};

// =========================================================

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

    // [ 윈도우 생성 ]
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"DX11ComponentGameClass";
    RegisterClassEx(&wcex);

    HWND hWnd = CreateWindow(L"DX11ComponentGameClass", L"DirectX 11 Game Engine - Component Pattern",
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
    sd.Windowed = TRUE; // 초기 창 모드

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    // [ 셰이더 및 레이아웃 초기화 ]
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

    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = 16;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    g_pd3dDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer);

    // =========================================================
    // [ 게임 오브젝트 인스턴스화 ]
    // =========================================================
    std::vector<std::unique_ptr<GameObject>> gameObjects;

    // Player 1 (오른쪽 배치, 붉은색, 방향키 조작)
    auto player1 = std::make_unique<GameObject>(0.4f, 0.0f);
    player1->AddComponent<RendererComponent>(1.0f, 0.2f, 0.2f); // Red
    player1->AddComponent<PlayerControllerComponent>(VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT);
    gameObjects.push_back(std::move(player1));

    // Player 2 (왼쪽 배치, 푸른색, WASD 조작)
    auto player2 = std::make_unique<GameObject>(-0.4f, 0.0f);
    player2->AddComponent<RendererComponent>(0.2f, 0.5f, 1.0f); // Blue
    player2->AddComponent<PlayerControllerComponent>('W', 'S', 'A', 'D');
    gameObjects.push_back(std::move(player2));


    // [ 고해상도 타이머(QPC) 설정 ]
    LARGE_INTEGER frequency, prevTime, currTime;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&prevTime);

    bool isRunning = true;
    bool isFullscreen = false;
    bool fKeyWasPressed = false;
    MSG msg = { 0 };

    // 콘솔 출력을 위한 누적 시간 및 프레임 카운트 변수
    double elapsedTime = 0.0;
    int frameCount = 0;

    // --- 메인 게임 루프 ---
    while (isRunning) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // 1. DeltaTime 계산
            QueryPerformanceCounter(&currTime);
            float dt = (float)(currTime.QuadPart - prevTime.QuadPart) / frequency.QuadPart;
            prevTime = currTime;

            // 1초마다 DeltaTime 및 FPS 콘솔 출력
            elapsedTime += dt;
            frameCount++;
            if (elapsedTime >= 1.0) {
                printf("DeltaTime: %.6f초 | FPS: %d\n", dt, frameCount);
                elapsedTime -= 1.0; // 누적 시간 1초 차감 (초기화)
                frameCount = 0;     // 프레임 초기화
            }

            // 2. 시스템 제어 입력 처리
            // ESC 종료
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                isRunning = false;
            }

            // F 키: 전체 화면 토글 (디바운싱 처리)
            bool fKeyIsPressed = (GetAsyncKeyState('F') & 0x8000) != 0;
            if (fKeyIsPressed && !fKeyWasPressed) {
                isFullscreen = !isFullscreen;
                g_pSwapChain->SetFullscreenState(isFullscreen, nullptr);
            }
            fKeyWasPressed = fKeyIsPressed;

            // 3. Update (게임 상태 업데이트)
            for (auto& obj : gameObjects) {
                obj->Update(dt);
            }

            // 4. Render (출력 단계)
            float clearColor[] = { 0.15f, 0.15f, 0.15f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0.0f, 1.0f };
            g_pImmediateContext->RSSetViewports(1, &vp);

            // 공통 셰이더 및 렌더링 파이프라인 상태 설정
            g_pImmediateContext->IASetInputLayout(g_pInputLayout);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
            g_pImmediateContext->VSSetShader(g_pVShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(g_pPShader, nullptr, 0);

            // 각 게임 오브젝트 렌더링(Component->Render 호출)
            for (auto& obj : gameObjects) {
                obj->Render();
            }

            g_pSwapChain->Present(1, 0); // VSync 유지
        }
    }

    // 자원 해제 (스마트 포인터를 사용한 GameObject 배열은 자동으로 소멸자가 호출되며 VertexBuffer 해제됨)
    gameObjects.clear();

    if (g_pConstantBuffer) g_pConstantBuffer->Release();
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pVShader) g_pVShader->Release();
    if (g_pPShader) g_pPShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();

    // 전체화면 상태에서 프로그램 종료 시 예외 방지
    if (g_pSwapChain) {
        g_pSwapChain->SetFullscreenState(FALSE, nullptr);
        g_pSwapChain->Release();
    }
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}