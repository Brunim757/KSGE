# KSGE — Kizuri Studio Game Engine

Micro-engine 3D genérica, exclusiva e de alta performance em **C++ moderno** para jogos de mundo aberto massivo com streaming.

## Stack

- **Gráficos:** Direct3D 11 puro (Feature Level 11_0, Shader Model 5.0)
- **Janela/Input:** GLFW ou SDL2 (expõe HWND para o DX11)
- **Editor:** Nuklear (Immediate Mode) com tema escuro autoral e ícones vetoriais
- **ECS:** Flecs
- **Matemática:** DirectXMath ou GLM
- **Assets:** fastgltf (.gltf/.glb)
- **Scripting:** Visual Node-Based → C# → runtime embutido do .NET 10 (CoreCLR via nethost/hostfxr)
- **Export:** executável nativo (.exe) independente

## Documentação

- `OPENCODE.md` — diretrizes oficiais do projeto
- `ROADMAP.md` — planejamento completo em fases

## Build

Todo build/teste é executado remotamente via **GitHub Actions** (ver `.github/workflows/`). Nada é compilado localmente.

| Workflow | Função |
|---|---|
| `ci.yml` | Build Debug/Release + smoke test real do executável; em tags `v*`, gera a Release com o artefato .exe |

## Status

Em desenvolvimento — ver `ROADMAP.md`.