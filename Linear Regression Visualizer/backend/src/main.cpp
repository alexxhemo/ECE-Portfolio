#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <tchar.h>

#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <random>
#include <chrono>
#include <algorithm>
#include <cstring>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "implot.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// ---------------------------------------------------------------------------
// Regression class
// ---------------------------------------------------------------------------
class Regression {
public:
    std::vector<float> x;
    std::vector<float> y;
    float slope = 0.0f;
    float intercept = 0.0f;
    bool needsUpdate = true;

    void addPoint(float a, float b) {
        x.push_back(a);
        y.push_back(b);
        needsUpdate = true;
    }

    void clear() {
        x.clear(); y.clear();
        slope = intercept = 0.0f;
        needsUpdate = true;
    }

    void compute() {
        if (x.size() < 2) return;
        float N = (float)x.size();
        float sum_x = 0, sum_y = 0;
        float sum_xy = 0, sum_x2 = 0;
        for (size_t i = 0; i < x.size(); ++i) {
            sum_x += x[i];
            sum_y += y[i];
            sum_xy += x[i] * y[i];
            sum_x2 += x[i] * x[i];
        }
        float denom = N * sum_x2 - sum_x * sum_x;
        if (denom == 0) return;
        slope = (N * sum_xy - sum_x * sum_y) / denom;
        intercept = (sum_y - slope * sum_x) / N;
        needsUpdate = false;
    }

    float predict(float a) const {
        return slope * a + intercept;
    }
};

// ---------------------------------------------------------------------------
// DirectX / ImGui globals & forward declarations (standard ImGui example code)
// ---------------------------------------------------------------------------
ID3D11Device* g_pd3dDevice = NULL;
ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
IDXGISwapChain* g_pSwapChain = NULL;
ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
bool CreateRenderTarget();
void CleanupRenderTarget();
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ---------------------------------------------------------------------------
// Win32 message handler
// ---------------------------------------------------------------------------
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// CSV loader (simple)
// ---------------------------------------------------------------------------
bool LoadCSV(const char* filename, Regression& reg) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;
    reg.clear();
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        float a, b;
        char comma;
        if (!(ss >> a)) continue;
        ss >> comma; // accept either comma or whitespace
        if (!(ss >> b)) continue;
        reg.addPoint(a, b);
    }
    reg.needsUpdate = true;
    return true;
}

// ---------------------------------------------------------------------------
// File dialog
// ---------------------------------------------------------------------------
bool OpenFileDialog(char outPath[MAX_PATH]) {
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "CSV Files\0*.csv\0All Files\0*.*\0";
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    outPath[0] = '\0';
    return GetOpenFileNameA(&ofn) == TRUE;
}

// ---------------------------------------------------------------------------
// Heatmap builder: bin points into rows x cols grid
// ---------------------------------------------------------------------------
void BuildHeatmap(const Regression& reg, std::vector<float>& heat, int rows, int cols,
    float xmin, float xmax, float ymin, float ymax)
{
    heat.assign(rows * cols, 0.0f);
    if (xmin >= xmax || ymin >= ymax) return;
    for (size_t i = 0; i < reg.x.size(); ++i) {
        float xv = reg.x[i];
        float yv = reg.y[i];
        if (xv < xmin || xv > xmax || yv < ymin || yv > ymax) continue;
        int cx = (int)std::floor((xv - xmin) / (xmax - xmin) * (cols - 1));
        int cy = (int)std::floor((yv - ymin) / (ymax - ymin) * (rows - 1));
        cx = std::max(0, std::min(cols - 1, cx));
        cy = std::max(0, std::min(rows - 1, cy));
        heat[cy * cols + cx] += 1.0f;
    }
}

// ---------------------------------------------------------------------------
// Helper: compute data bounds (min/max). If no data, returns defaults.
// ---------------------------------------------------------------------------
void ComputeBounds(const Regression& reg, float& xmin, float& xmax, float& ymin, float& ymax) {
    if (reg.x.empty()) {
        xmin = 0.0f; xmax = 10.0f;
        ymin = 0.0f; ymax = 10.0f;
        return;
    }
    xmin = *std::min_element(reg.x.begin(), reg.x.end());
    xmax = *std::max_element(reg.x.begin(), reg.x.end());
    ymin = *std::min_element(reg.y.begin(), reg.y.end());
    ymax = *std::max_element(reg.y.begin(), reg.y.end());
    // add small padding
    float px = (xmax - xmin) * 0.05f + 1e-3f;
    float py = (ymax - ymin) * 0.05f + 1e-3f;
    xmin -= px; xmax += px; ymin -= py; ymax += py;
}

// ---------------------------------------------------------------------------
// Main: Real-time simulation integrated
// ---------------------------------------------------------------------------
int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // Register class & create window
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0,0, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("LRSimWindow"), NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, _T("Linear Regression Simulator (ImGui + ImPlot)"),
        WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    CreateRenderTarget();
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // ImGui + ImPlot init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Simulation & regression objects
    Regression reg;
    // start with some seed points
    reg.addPoint(1, 2);
    reg.addPoint(2, 4);
    reg.addPoint(3, 5);

    // random generation setup
    std::mt19937 rng((unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::normal_distribution<float> noise_dist(0.0f, 1.0f);

    // simulation control state
    bool sim_running = true;
    float points_per_second = 5.0f;   // spawn rate
    float sim_true_slope = 1.0f;
    float sim_true_intercept = 1.0f;
    float sim_noise_std = 0.5f;
    float elapsed_acc = 0.0f;
    const float max_sim_dt = 0.5f; // cap timestep for stability
    int max_points = 5000;

    // heatmap params
    int heat_rows = 30;
    int heat_cols = 30;
    std::vector<float> heat;

    // tracking time
    auto last_time = std::chrono::high_resolution_clock::now();

    // loaded file name buffer
    char loadedFile[MAX_PATH] = "None";

    // control for x generation - we'll increase x monotonically
    float next_x = 4.0f;

    // main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT)
    {
        // Poll Windows messages
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // Timing
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> dt_s = now - last_time;
        float dt = dt_s.count();
        last_time = now;
        if (dt > max_sim_dt) dt = max_sim_dt;

        // Simulation step (generate points based on spawn rate)
        if (sim_running) {
            elapsed_acc += dt;
            float spawn_interval = 1.0f / std::max(0.0001f, points_per_second);
            while (elapsed_acc >= spawn_interval) {
                elapsed_acc -= spawn_interval;
                // generate a new point: y = true_slope * x + intercept + noise
                float noise = noise_dist(rng) * sim_noise_std;
                float yv = sim_true_slope * next_x + sim_true_intercept + noise;
                reg.addPoint(next_x, yv);
                next_x += 1.0f; // step x; you can adjust to smaller increments if you want
                // guard maximum size
                if (reg.x.size() > (size_t)max_points) {
                    // simple pop-front to keep buffer size (slow for vector but okay for small sizes)
                    reg.x.erase(reg.x.begin());
                    reg.y.erase(reg.y.begin());
                }
            }
        }

        // Begin ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ---------------------------
        // Controls window
        // ---------------------------
        ImGui::Begin("Simulation Controls");

        if (ImGui::Button(sim_running ? "Pause" : "Resume")) {
            sim_running = !sim_running;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            reg.clear();
            next_x = 0.0f;
            elapsed_acc = 0.0f;
            strcpy_s(loadedFile, "None");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load CSV")) {
            char path[MAX_PATH];
            if (OpenFileDialog(path)) {
                if (LoadCSV(path, reg)) {
                    strcpy_s(loadedFile, path);
                    // update next_x so generated points continue from max x + 1
                    if (!reg.x.empty()) {
                        float mx = *std::max_element(reg.x.begin(), reg.x.end());
                        next_x = mx + 1.0f;
                    }
                    else next_x = 0.0f;
                }
            }
        }
        ImGui::Text("Loaded File: %s", loadedFile);
        ImGui::Separator();

        ImGui::SliderFloat("Points / sec", &points_per_second, 0.0f, 200.0f, "%.1f");
        ImGui::SliderFloat("Noise StdDev", &sim_noise_std, 0.0f, 5.0f, "%.2f");
        ImGui::SliderFloat("True Slope", &sim_true_slope, -5.0f, 5.0f, "%.3f");
        ImGui::SliderFloat("True Intercept", &sim_true_intercept, -20.0f, 20.0f, "%.3f");
        ImGui::InputInt("Max Points Buffer", &max_points);
        if (max_points < 10) max_points = 10;
        ImGui::Separator();

        // manual point input
        static float manual_x = 0.0f, manual_y = 0.0f;
        ImGui::InputFloat("Manual X", &manual_x);
        ImGui::InputFloat("Manual Y", &manual_y);
        if (ImGui::Button("Add Manual Point")) {
            reg.addPoint(manual_x, manual_y);
        }

        ImGui::Separator();
        ImGui::Text("Dataset: %zu points", reg.x.size());
        if (reg.needsUpdate) reg.compute();
        ImGui::Text("Regression: y = %.4fx + %.4f", reg.slope, reg.intercept);
        ImGui::End();

        // ---------------------------
        // Plot window
        // ---------------------------
        ImGui::Begin("Regression Plot");
        if (ImPlot::BeginPlot("Data + Regression")) {
            ImPlot::SetupAxes("X", "Y");

            if (!reg.x.empty())
                ImPlot::PlotScatter("Data", reg.x.data(), reg.y.data(), (int)reg.x.size());

            if (!reg.needsUpdate) {
                // choose plot range from data bounds
                float xmin, xmax, ymin, ymax;
                ComputeBounds(reg, xmin, xmax, ymin, ymax);
                float xline0 = xmin;
                float xline1 = xmax;
                float yline0 = reg.predict(xline0);
                float yline1 = reg.predict(xline1);
                float xline[2] = { xline0, xline1 };
                float yline[2] = { yline0, yline1 };
                ImPlot::PlotLine("Fit", xline, yline, 2);
            }

            ImPlot::EndPlot();
        }
        ImGui::End();

        // ---------------------------
        // Heatmap window
        // ---------------------------
        ImGui::Begin("Heatmap");
        ImGui::SliderInt("Heat rows", &heat_rows, 4, 200);
        ImGui::SliderInt("Heat cols", &heat_cols, 4, 200);
        // build heatmap
        float xmin, xmax, ymin, ymax;
        ComputeBounds(reg, xmin, xmax, ymin, ymax);
        BuildHeatmap(reg, heat, heat_rows, heat_cols, xmin, xmax, ymin, ymax);
        float maxval = 0.0f;
        for (float v : heat) if (v > maxval) maxval = v;
        if (maxval <= 0.0f) maxval = 1.0f; // avoid zero scale
        if (ImPlot::BeginPlot("Density")) {
            ImPlot::PlotHeatmap("DensityHeat", heat.data(), heat_rows, heat_cols,
                0.0, maxval, nullptr,
                ImPlotPoint(xmin, ymin), ImPlotPoint(xmax, ymax));
            ImPlot::EndPlot();
        }
        ImGui::End();

        // ---------------------------
        // Optional: simple histogram of residuals
        // ---------------------------
        ImGui::Begin("Residuals");
        if (!reg.x.empty() && !reg.needsUpdate) {
            // compute residuals
            std::vector<float> residuals;
            residuals.reserve(reg.x.size());
            for (size_t i = 0; i < reg.x.size(); ++i) {
                float r = reg.y[i] - reg.predict(reg.x[i]);
                residuals.push_back(r);
            }
            // create a quick histogram into 50 bins
            int bins = 50;
            std::vector<float> hist(bins, 0.0f);
            float rmin = *std::min_element(residuals.begin(), residuals.end());
            float rmax = *std::max_element(residuals.begin(), residuals.end());
            if (rmin == rmax) { rmin -= 0.5f; rmax += 0.5f; }
            for (float r : residuals) {
                int bi = (int)((r - rmin) / (rmax - rmin) * (bins - 1));
                bi = std::max(0, std::min(bins - 1, bi));
                hist[bi] += 1.0f;
            }
            if (ImPlot::BeginPlot("Residual Histogram")) {
                // x values as bin centers
                std::vector<float> bx(bins);
                std::vector<float> by(bins);
                for (int i = 0; i < bins; ++i) {
                    bx[i] = rmin + (rmax - rmin) * (i / (float)bins);
                    by[i] = hist[i];
                }
                ImPlot::PlotBars("Residuals", bx.data(), by.data(), bins, 0.0f);
                ImPlot::EndPlot();
            }
        }
        else {
            ImGui::Text("No residuals (need >=2 points)");
        }
        ImGui::End();

        // ---------------------------
        // Finish frame and render
        // ---------------------------
        ImGui::Render();
        const float clear_col[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_col);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    // Cleanup
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    CleanupRenderTarget();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 0;
}

// NOTE: DirectX helper implementations are provided in src/d3d_helpers.cpp
// The prototypes are declared earlier in this file and the implementations
// must not be duplicated here to avoid multiple-definition linker errors.
