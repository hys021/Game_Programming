#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <windows.h>
#include <vector>
#include <string>
#include <chrono>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <iostream>

// [비디오 설정 관리 구조체]
struct VideoConfig {
  int Width = 800;
  int Height = 600;
  bool IsFullscreen = false;
  bool NeedsResize = false;
  int VSync = 1;
} g_Config;

// [DirectX 전역 자원]
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;

struct Vertex {
  float x, y, z;
  float r, g, b, a;
};

// --- [해상도 및 리소스 재구성 함수] ---
void RebuildVideoResources(HWND hWnd)
{
  if (!g_pSwapChain) return;

  // 1. 기존 렌더 타겟 뷰 해제 (안 하면 ResizeBuffers 실패함)
  if (g_pRenderTargetView)
  {
    g_pRenderTargetView->Release();
    g_pRenderTargetView = nullptr;
  }

  // 2. 백버퍼 크기 재설정
  g_pSwapChain->ResizeBuffers(0, g_Config.Width, g_Config.Height, DXGI_FORMAT_UNKNOWN, 0);

  // 3. 새 백버퍼로부터 렌더 타겟 뷰 다시 생성
  ID3D11Texture2D* pBackBuffer = nullptr;
  g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
  if (pBackBuffer == nullptr)
  {
    printf("GETBUFFER ERROR\n");
    return;
  }
  g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
  pBackBuffer->Release();

  // 4. 윈도우 창 크기 실제 조정 (전체화면이 아닐 때만)
  if (!g_Config.IsFullscreen)
  {
    RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    SetWindowPos(hWnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
  }

  g_Config.NeedsResize = false;
  printf("[Video] Changed: %d x %d\n", g_Config.Width, g_Config.Height);
}

// ==============================================================================
// [엔진 설계: Component & GameObject]
// ==============================================================================
class Component {
public:
  class GameObject* pOwner = nullptr;
  bool isStarted = false;
  virtual void Start() = 0;
  virtual void Input() {}
  virtual void Update(float dt) = 0;
  virtual void Render() {}
  virtual ~Component() {}
};

class GameObject {
public:
  std::string name;
  float posX = 0.0f; float posY = 0.0f;
  std::vector<Component*> components;

  GameObject(std::string n) : name(n) {}
  ~GameObject() { for (auto c : components) delete c; }
  void AddComponent(Component* pComp) {
    pComp->pOwner = this;
    components.push_back(pComp);
  }
};

// ==============================================================================
// [컴포넌트 구현: 조종 및 렌더러]
// ==============================================================================
class Player1Control : public Component {
public:
  float velX = 0.0f, velY = 0.0f, speed = 1.5f;
  void Start() override {}
  void Input() override {
    velX = 0.0f; velY = 0.0f;
    if (GetAsyncKeyState(VK_UP) & 0x8000)    velY = speed;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)  velY = -speed;
    if (GetAsyncKeyState(VK_LEFT) & 0x8000)  velX = -speed;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) velX = speed;
  }
  void Update(float dt) override { pOwner->posX += velX * dt; pOwner->posY += velY * dt; }
};

class Player2Control : public Component {
public:
  float velX = 0.0f, velY = 0.0f, speed = 1.5f;
  void Start() override {}
  void Input() override {
    velX = 0.0f; velY = 0.0f;
    if (GetAsyncKeyState('W') & 0x8000) velY = speed;
    if (GetAsyncKeyState('S') & 0x8000) velY = -speed;
    if (GetAsyncKeyState('A') & 0x8000) velX = -speed;
    if (GetAsyncKeyState('D') & 0x8000) velX = speed;
  }
  void Update(float dt) override { pOwner->posX += velX * dt; pOwner->posY += velY * dt; }
};

class TriangleRenderer : public Component {
public:
  float r, g, b;
  ID3D11Buffer* pVB = nullptr;
  TriangleRenderer(float _r, float _g, float _b) : r(_r), g(_g), b(_b) {}
  void Start() override {
    D3D11_BUFFER_DESC bd = { sizeof(Vertex) * 3, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, nullptr, &pVB);
  }
  void Update(float dt) override {}
  void Render() override {
    Vertex v[3] = { { pOwner->posX + 0.0f, pOwner->posY + 0.15f, 0.0f, r, g, b, 1.0f },
                    { pOwner->posX + 0.15f, pOwner->posY - 0.15f, 0.0f, r, g, b, 1.0f },
                    { pOwner->posX - 0.15f, pOwner->posY - 0.15f, 0.0f, r, g, b, 1.0f } };
    D3D11_MAPPED_SUBRESOURCE ms;
    g_pImmediateContext->Map(pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, v, sizeof(v));
    g_pImmediateContext->Unmap(pVB, 0);
    UINT stride = sizeof(Vertex), offset = 0;
    g_pImmediateContext->IASetVertexBuffers(0, 1, &pVB, &stride, &offset);
    g_pImmediateContext->Draw(3, 0);
  }
  ~TriangleRenderer() { if (pVB) pVB->Release(); }
};

// ==============================================================================
// [엔진 코어: GameLoop]
// ==============================================================================
class GameLoop {
public:
  bool isRunning = true;
  std::vector<GameObject*> gameWorld;
  std::chrono::high_resolution_clock::time_point prevTime;

  void Run(HWND hWnd) {
    prevTime = std::chrono::high_resolution_clock::now();
    MSG msg = { 0 };
    while (isRunning && msg.message != WM_QUIT) {
      if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg); DispatchMessage(&msg);
      }
      else {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - prevTime).count();
        prevTime = now;

        if (GetAsyncKeyState(VK_ESCAPE) & 0x0001) isRunning = false;
        if (GetAsyncKeyState('F') & 0x0001) {
          g_Config.IsFullscreen = !g_Config.IsFullscreen;
          g_pSwapChain->SetFullscreenState(g_Config.IsFullscreen, nullptr);
        }

        for (auto obj : gameWorld) for (auto comp : obj->components) {
          if (!comp->isStarted) { comp->Start(); comp->isStarted = true; }
          comp->Input(); comp->Update(dt);
        }

        float clearColor[] = { 0.1f, 0.15f, 0.2f, 1.0f };
        g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
        D3D11_VIEWPORT vp = { 0, 0, (float)g_Config.Width, (float)g_Config.Height, 0, 1 };
        g_pImmediateContext->RSSetViewports(1, &vp);
        g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
        g_pImmediateContext->IASetInputLayout(g_pInputLayout);
        g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
        g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

        for (auto obj : gameWorld) for (auto comp : obj->components) comp->Render();
        g_pSwapChain->Present(g_Config.VSync, 0);
      }
    }
  }
  ~GameLoop() { for (auto obj : gameWorld) delete obj; }
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_DESTROY) PostQuitMessage(0);
  return DefWindowProc(hWnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
  WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hI, 0, LoadCursor(0, IDC_ARROW), 0, 0, L"DXHW", 0 };
  RegisterClassExW(&wc);
  RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
  AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, FALSE);
  HWND hWnd = CreateWindowW(L"DXHW", L"Lecture04-HW", WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, 100, 100, rc.right - rc.left, rc.bottom - rc.top, 0, 0, hI, 0);

  DXGI_SWAP_CHAIN_DESC sd = { { (UINT)g_Config.Width, (UINT)g_Config.Height, { 60, 1 }, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED, DXGI_MODE_SCALING_UNSPECIFIED }, { 1, 0 }, DXGI_USAGE_RENDER_TARGET_OUTPUT, 1, hWnd, TRUE, DXGI_SWAP_EFFECT_DISCARD, 0 };
  D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0, 0, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, 0, &g_pImmediateContext);
  RebuildVideoResources(hWnd);

  const char* s = "struct IA { float3 p : POSITION; float4 c : COLOR; }; struct PS { float4 p : SV_POSITION; float4 c : COLOR; }; PS VS(IA i) { PS o; o.p = float4(i.p, 1); o.c = i.c; return o; } float4 PS_Main(PS i) : SV_Target { return i.c; }";
  ID3DBlob* vB, * pB; D3DCompile(s, strlen(s), 0, 0, 0, "VS", "vs_4_0", 0, 0, &vB, 0); D3DCompile(s, strlen(s), 0, 0, 0, "PS_Main", "ps_4_0", 0, 0, &pB, 0);
  g_pd3dDevice->CreateVertexShader(vB->GetBufferPointer(), vB->GetBufferSize(), 0, &g_pVertexShader);
  g_pd3dDevice->CreatePixelShader(pB->GetBufferPointer(), pB->GetBufferSize(), 0, &g_pPixelShader);
  D3D11_INPUT_ELEMENT_DESC ied[] = { { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
  g_pd3dDevice->CreateInputLayout(ied, 2, vB->GetBufferPointer(), vB->GetBufferSize(), &g_pInputLayout);
  vB->Release(); pB->Release(); ShowWindow(hWnd, nS);

  GameLoop gLoop;
  GameObject* p1 = new GameObject("P1"); p1->posX = -0.4f;
  p1->AddComponent(new Player1Control()); p1->AddComponent(new TriangleRenderer(1, 0.3f, 0.3f));
  gLoop.gameWorld.push_back(p1);

  GameObject* p2 = new GameObject("P2"); p2->posX = 0.4f;
  p2->AddComponent(new Player2Control()); p2->AddComponent(new TriangleRenderer(0.3f, 1, 0.3f));
  gLoop.gameWorld.push_back(p2);

  gLoop.Run(hWnd);
  return 0;
}