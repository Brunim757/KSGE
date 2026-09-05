# 🗺️ ROADMAP MASTER — KSGE (Kizuri Studio Game Engine)

> Documento oficial de planejamento, execução e governança do projeto KSGE.
> Arquivo complementar: `OPENCODE.md`.
> Stack restrita: **C++ moderno + Direct3D 11 puro + Nuklear + Flecs + fastgltf + .NET 10 (CoreCLR)**.

---

## 📌 ÍNDICE GERAL

- [1. Visão Geral do Projeto](#1-visão-geral-do-projeto)
- [2. Arquitetura Máster](#2-arquitetura-máster)
- [3. Mapa de Fases do Desenvolvimento](#3-mapa-de-fases-do-desenvolvimento)
- [4. Fase 0 — Fundação e Planejamento](#4-fase-0--fundação-e-planejamento)
- [5. Fase 1 — Repositório, CI/CD e Governança GitHub](#5-fase-1--repositório-cicd-e-governança-github)
- [6. Fase 2 — Bootstrap Gráfico (Janela + DX11 + Flecs)](#6-fase-2--bootstrap-gráfico-janela--dx11--flecs)
- [7. Fase 3 — Matemática 3D e Câmara](#7-fase-3--matemática-3d-e-câmara)
- [8. Fase 4 — Carregamento de Assets (gltf/glb via fastgltf)](#8-fase-4--carregamento-de-assets-gltfglb-via-fastgltf)
- [9. Fase 5 — Pipeline de Renderização PBR](#9-fase-5--pipeline-de-renderização-pbr)
- [10. Fase 6 — Pós-Processamento Avançado](#10-fase-6--pós-processamento-avançado)
- [11. Fase 7 — Iluminação Global em Software](#11-fase-7--iluminação-global-em-software)
- [12. Fase 8 — Instanced Rendering e Otimização GT 610](#12-fase-8--instanced-rendering-e-otimização-gt-610)
- [13. Fase 9 — Mundo Aberto, Chunks e Streaming Assíncrono](#13-fase-9--mundo-aberto-chunks-e-streaming-assíncrono)
- [14. Fase 10 — Editor Nuklear (Shell da Interface)](#14-fase-10--editor-nuklear-shell-da-interface)
- [15. Fase 11 — Aba: World Outliner](#15-fase-11--aba-world-outliner)
- [16. Fase 12 — Aba: Inspector](#16-fase-12--aba-inspector)
- [17. Fase 13 — Aba: World Streamer](#17-fase-13--aba-world-streamer)
- [18. Fase 14 — Aba: Visual Scripting Canvas](#18-fase-14--aba-visual-scripting-canvas)
- [19. Fase 15 — Painel: Content Browser](#19-fase-15--painel-content-browser)
- [20. Fase 16 — Painel: Output Console](#20-fase-16--painel-output-console)
- [21. Fase 17 — Input, Atalhos e Gizmos](#21-fase-17--input-atalhos-e-gizmos)
- [22. Fase 18 — Integração .NET 10 (CoreCLR Hosting)](#22-fase-18--integração-net-10-coreclr-hosting)
- [23. Fase 19 — Geração de Código C# a partir de Nós](#23-fase-19--geração-de-código-c-a-partir-de-nós)
- [24. Fase 20 — Compilação e Exportação do .exe Final](#24-fase-20--compilação-e-exportação-do-exe-final)
- [25. Fase 21 — Tema Escuro Autoral e Ícones Vetoriais](#25-fase-21--tema-escuro-autoral-e-ícones-vetoriais)
- [26. Fase 22 — Polimento, Perf e Testes de Qualidade](#26-fase-22--polimento-perf-e-testes-de-qualidade)
- [27. Fase 23 — Empacotamento e Lançamento (Release)](#27-fase-23--empacotamento-e-lançamento-release)
- [28. Roadmap Pós-Versão 1.0 (V2)](#28-roadmap-pós-versão-10-v2)
- [29. Dicionário Técnico e Glossário](#29-dicionário-técnico-e-glossário)
- [30. Definição de Pronto (DoD) Global](#30-definição-de-pronto-dod-global)
- [31. Convenções de Código e Estilo](#31-convenções-de-código-e-estilo)
- [32. Risco / Gestão de Dependências](#32-risco--gestão-de-dependências)
- [33. Ambiente de Teste (GT 610) — Matriz de Qualidade](#33-ambiente-de-teste-gt-610--matriz-de-qualidade)
- [34. Comandos úteis do GitHub CLI](#34-comandos-úteis-do-github-cli)
- [35. Checklist de Entrega Final](#35-checklist-de-entrega-final)
- [36. Log de Decisões Arquiteturais (ADR)](#36-log-de-decisões-arquiteturais-adr)

---

## 1. Visão Geral do Projeto

### 1.1 O que é a KSGE

A **KSGE (Kizuri Studio Game Engine)** é uma micro-engine 3D genérica, exclusiva e de alta performance escrita em **C++ moderno**, projetada para jogos de **mundo aberto massivo com streaming**. A engine roda sobre **API nativa Direct3D 11** no Windows, entrega um **editor visual imediate-mode baseado em Nuklear**, suporta **Scripting Visual Node-Based** e exporta o jogo final como um **executável nativo independante (.exe)** em uma única etapa.

### 1.2 Missão do Projeto

Oferecer aos criadores independentes um estúdio completo (editor + runtime) capaz de:
- Construir mundos abertos infinitos com carregamento em segundo plano (streaming).
- Criar gameplay sem digitar código textual (visual scripting node-based).
- Gerar executáveis nativos de alta performance sem dependências externas.
- Rodar em hardware modesto (objetivo mínimo de teste: **GT 610**).

### 1.3 Pilares Inegociáveis

1. **Performance:** Data-Oriented Design, zero alocação desnecessária em hot loop, instanced rendering.
2. **Exclusividade visual:** Tema escuro autoral, ícones vetoriais, UX única.
3. **Automação total:** Build/teste 100% via GitHub Actions (sem construir localmente).
4. **Código limpo:** Proibido comentários em código-fonte.
5. **Portabilidade do produto:** Executável `.exe` final independente e otimizado.

### 1.4 Escopo Funcional (MVP → Release 1.0)

| Área | Escopo v1.0 |
|---|---|
| Janela / Input | GLFW expondo HWND para DX11 |
| Renderização | DX11 Feature Level 11_0, shaders SM 5.0 |
| Materiais | PBR (albedo/roughness/metallic/normal) |
| Iluminação | CSM, SSGI, SSR, SSAO, Volumetric Fog |
| Pós-processo | Bloom físico, Color Grading LUT |
| Mundo | Chunks infinitos, streaming assíncrono |
| ECS | Flecs como backbone de todos os dados |
| Modelos | fastgltf (.gltf/.glb) |
| Editor | Nuklear: 4 abas + 2 painéis + gizmos |
| Scripting | Visual Node → C# → CoreCLR executado |
| Export | .exe via Native AOT |
| CI | GitHub Actions para tudo |

---

## 2. Arquitetura Máster

### 2.1 Camadas da Engine (Topo → Base)

```
┌─────────────────────────────────────────────────────────────┐
│  KIZURI EDITOR (Nuklear UI)                                 │
│  ─ Abas Superiores · Painéis inferiores · Barra de gadgets   │
└─────────────────────────┬───────────────────────────────────┘
┌─────────────────────────▼───────────────────────────────────┐
│  KIZURI RUNTIME (Play Mode)                                 │
│  ─ Gameplay .NET 10 · Systems Flecs · Streaming · Físico    │
└─────────────────────────┬───────────────────────────────────┘
┌─────────────────────────▼───────────────────────────────────┐
│  RENDERER CORE (DX11)                                       │
│  ─ PBR · CSM · SSGI · SSR · SSAO · Fog · Bloom · LUT · UI   │
└─────────────────────────┬───────────────────────────────────┘
┌─────────────────────────▼───────────────────────────────────┐
│  PLATFORM (Windows)                                         │
│  ─ GLFW (HWND) · DX11 · nethost/hostfxr · Windows SDK       │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Fluxo de Dados no Mundo Aberto

```
[Player se move] → [Posição da Câmara]
    → [Sistema de Streaming decide chunk a carregar/descarregar]
    → [Thread de IO lê .gltf + metadados binários do SSD]
    → [Flecs cria entidades dos chunks carregados]
    → [Renderer chama DrawIndexedInstanced por tipo de mesh]
    → [GPU pinta a cena]
```

### 2.3 Fluxo do Visual Scripting

```
[Canvas de nós no Nuklear]
    → [Leitura sequencial do grafo em C++]
    → [Tradução para um arquivo .cs válido]
    → [CoreCLR compila em background (AOT)]
    → [Script atrelado a entidade via Flecs]
    → [Inspector varre campos públicos e expõe ao editor]
```

### 2.4 Estrutura de Diretórios Alvo do Repositório

```
/KSGE
├── .github/workflows/
│   ├── ci.yml
│   ├── build-editor.yml
│   ├── build-export.yml
│   └── release.yml
├── engine/
│   ├── core/              // Flecs world, ECS systems, services
│   ├── platform/          // GLFW, HWND, DLL loading (.NET hosting)
│   ├── graphics/          // DX11 device, swapchain, states, buffers
│   ├── shaders/           // HLSL SM5 (PBR, SSGI, CSM, etc.)
│   ├── scene/             // câmara, transform, gizmos, picking
│   ├── world/             // chunks, streaming, persistence binária
│   ├── assets/            // fastgltf loader, texturas, atlas de ícones
│   ├── scripting/         // node graph → C#, hostfxr/nethost
│   └── ui/                // Nuklear integration + panels + theme
├── editor/                // main() do editor, toolbars, viewport
├── runtime/               // runtime de jogabilidade (Play)
├── exporter/              // pipeline de geração do .exe final
├── data/
│   ├── assets/            // pastas do estúdio
│   ├── maps/              // binário serializado dos chunks
│   └── icons/             // .ttf de ícones vetoriais
├── scripts/               // ferramentas de automação
└── ROADMAP.md / OPENCODE.md
```

---

## 3. Mapa de Fases do Desenvolvimento

> Cada fase possui: **Objetivo**, **Entregáveis**, **Tarefas detalhadas**, **Critérios de aceite**, **Riscos**.

| Fase | Nome | Dependência |
|---|---|---|
| 0 | Fundação e Planejamento | — |
| 1 | Repositório, CI/CD e Governança GitHub | 0 |
| 2 | Bootstrap Gráfico (Janela + DX11 + Flecs) | 1 |
| 3 | Matemática 3D e Câmara | 2 |
| 4 | Carregamento de Assets | 2 |
| 5 | Pipeline de Renderização PBR | 3, 4 |
| 6 | Pós-Processamento Avançado | 5 |
| 7 | Iluminação Global em Software | 5, 6 |
| 8 | Instanced Rendering e Otimização GT 610 | 5 |
| 9 | Mundo Aberto, Chunks e Streaming | 4 |
| 10 | Editor Nuklear (Shell da Interface) | 2 |
| 11 | Aba: World Outliner | 10, 9 |
| 12 | Aba: Inspector | 10, 11, 22 |
| 13 | Aba: World Streamer | 10, 9 |
| 14 | Aba: Visual Scripting Canvas | 10, 18 |
| 15 | Painel: Content Browser | 10 |
| 16 | Painel: Output Console | 10 |
| 17 | Input, Atalhos e Gizmos | 10, 2 |
| 18 | Integração .NET 10 (CoreCLR Hosting) | 2 |
| 19 | Geração de Código C# a partir de Nós | 14, 18 |
| 20 | Compilação e Exportação do .exe Final | 19 |
| 21 | Tema Escuro Autoral e Ícones Vetoriais | 10 |
| 22 | Polimento, Perf e Testes de Qualidade | Todas |
| 23 | Empacotamento e Lançamento (Release) | 22 |

---

## 4. Fase 0 — Fundação e Planejamento

### 4.1 Objetivo

Consolidar todas as decisões técnicas, documentação e normas antes de escrever qualquer linha de código.

### 4.2 Entregáveis

- `OPENCODE.md` atualizado (nome correto: Kizuri Studio).
- `ROADMAP.md` (este arquivo) como fonte única de planejamento.
- Decisões de stack registradas em ADR (seção 36).
- Checklist de ambiente (somente leitura/validação), pois build é via GitHub.

### 4.3 Tarefas Detalhadas

- [ ] Validar o nome oficial: **Kizuri Studio Game Engine (KSGE)**.
- [ ] Confirmar sistema operacional alvo (Windows 10/11, DX11 Feature Level 11_0).
- [ ] Confirmar versões: C++ moderna (C++20/23), .NET 10, Flecs, Nuklear, fastgltf, GLFW/SDL2.
- [ ] Definir policy de licenciamento do engine e dos assets do estúdio.
- [ ] Definir padrão de commits e mensagens (Conventional Commits).
- [ ] Definir nome dos branches e fluxo (trunk-based ou Git Flow simplificado).
- [ ] Mapear hardware mínimo de teste (GT 610, RAM, SSD/HDD).
- [ ] Registrar decisões arquiteturais em ADR.

### 4.4 Critérios de Aceite

- Nome oficial correto em todos os documentos.
- Stack integralmente documentada.
- PAPEL: nenhuma dependência de build local (tudo via GitHub Actions).

### 4.5 Riscos

- Risco: executar build local por engano → mitigação: documentar e configurar CI como única via.
- Risco: escopo gigante sem milestone → mitigação: fases pequenas com aceite.

---

## 5. Fase 1 — Repositório, CI/CD e Governança GitHub

### 5.1 Objetivo

Criar o repositório do projeto, configurar GitHub Actions para buildar, testar e validar tudo remotamente, e estabelecer uma trilha de automação que a IA usará no dia a dia.

### 5.2 Entregáveis

- Repositório GitHub criado via `gh`.
- `.github/workflows/` com 4 pipelines completos.
- Secrets/config de runner Windows para DX11.
- Documentação de comandos `gh` no seção 34.

### 5.3 Tarefas Detalhadas

- [ ] Criar repositório privado via `gh repo create`.
- [ ] Configurar branch de proteção (main) com checks obrigatórios.
- [ ] Criar `ci.yml`: checkout, instalar Windows SDK/CMake/toolchain MSVC, build Debug/Release, rodar testes unitários.
- [ ] Criar `build-editor.yml`: build do editor completo.
- [ ] Criar `build-export.yml`: build do exporter + validação do .exe final.
- [ ] Criar `release.yml`: tag semver, geração de artefato .zip/.exe, publicação de Release.
- [ ] Configurar cache de dependências (vcpkg/conan) para acelerar builds.
- [ ] Configurar hardware runner Windows (windows-latest ou self-hosted).
- [ ] Testar falha proposital para validar notificações/status badge.

### 5.4 Critérios de Aceite

- Um push gera build completo nos servidores do GitHub.
- Nenhum .exe é gerado localmente.
- Release Draft é criado automaticamente na tag.
- Pipelines verdes em `main`.

### 5.5 Comandos GitHub CLI Utilizados

Ver seção 34 para o catálogo completo.

---

## 6. Fase 2 — Bootstrap Gráfico (Janela + DX11 + Flecs)

### 6.1 Objetivo

Ter um executável que abre uma janela via GLFW/SDL2, inicializa um device DX11 (Feature Level 11_0) com swapchain, limpa a tela, e inicializa o mundo Flecs com um loop de update estável.

### 6.2 Entregáveis

- Janela nativa com HWND exposto.
- DX11 device/context/swapchain criados.
- Backbuffer de renderização + depth buffer.
- Loop principal com delta time estável.
- World Flecs criado e rodando sistemas vazios.

### 6.3 Tarefas Detalhadas

- [ ] Inicializar GLFW; criar janela com atributos de API em branco (Graphics API via DX).
- [ ] Recuperar o `HWND` da janela GLFW (`glfwGetWin32Window`) e entregá-lo ao DX11.
- [ ] Criar `ID3D11Device`, `ID3D11DeviceContext`, `IDXGISwapChain` com Feature Level 11_0 (validação com `D3D_FEATURE_LEVEL_11_0`).
- [ ] Criar render target view do backbuffer.
- [ ] Criar depth stencil view.
- [ ] Configurar Blend State, Rasterizer State e Depth-Stencil State padrão (data-oriented, estados reutilizados).
- [ ] Implementar loop: `poll events → update Flecs → clear → render → swap buffers`.
- [ ] Implementar resize da janela (reconstrução limpa da swapchain).
- [ ] Criar sistema `FrametimeSystem` no Flecs para medir delta.
- [ ] Rodar o pipeline de CI para validar a primeira compilação DX11 no runner Windows.

### 6.4 Critérios de Aceite

- Janela 1280x720 abre e mantém 60 FPS em GT 610.
- Trocar resize sem crash.
- Flecs update roda a cada frame.
- CI compila com sucesso no Windows runner.

### 6.5 Riscos

- DX11 em runners do GitHub: usar `windows-latest` que suporta WARP para testes headless → implementar fallback WARP em CI.

---

## 7. Fase 3 — Matemática 3D e Câmara

### 7.1 Objetivo

Criar a camada matemática (DirectXMath ou GLM) e um sistema de câmara funcional com navegação por quaternion, FOV, near/far e controls de câmera livre.

### 7.2 Entregáveis

- Módulo de vetores/matrizes/quaternions.
- Câmara (posição, look-at, projeção perspectiva).
- Frustum classes para culling futuro.
- Raycast a partir do cursor (usado depois no picking).

### 7.3 Tarefas Detalhadas

- [ ] Integrar DirectXMath (alinhado com DX11) — preferencial.
- [ ] Criar `Camera` como componente Flecs (posição, yaw, pitch, fov, near, far).
- [ ] Implementar `frustumFromCamera` com 6 planos e teste de AABB.
- [ ] Implementar `screenToWorldRay` (usado no gizmo/picking).
- [ ] Implementar `moveAndLook` (W/A/S/D + mouse) para o modo câmera livre do editor.
- [ ] Acumular matriz view-projection por frame no buffer constant de cena.

### 7.4 Critérios de Aceite

- Câmara livre já roda no editor (sem UI ainda).
- Raycast retorna coordenadas corretas projetadas/desprojetadas.
- Frustum culling funcional (mesmo sem cena).

---

## 8. Fase 4 — Carregamento de Assets (gltf/glb via fastgltf)

### 8.1 Objetivo

Carregar malhas, materiais e texturas de arquivos `.gltf`/`.glb` em estruturas de GPU otimizadas, prontas para instancing.

### 8.2 Entregáveis

- Asset Registry (banco global de meshes/texturas/materiais em RAM).
- Loader fastgltf com dados de malha em buffers DX11.
- Pipeline de importação de texturas (png/dds) com mipmaps.
- Cache de conversão (hash do arquivo → asset ID).

### 8.3 Tarefas Detalhadas

- [ ] Integrar fastgltf (header/amalgamado ou vcpkg).
- [ ] Parser: extrair buffers, bufferViews, accessors, meshes, nodes, materiais.
- [ ] Converter accessors (posição, normal, UV, tangente, cor).
- [ ] Compactar índices e vértices em um único vertex buffer interleaved para draw call otimizada.
- [ ] Criar pipeline de texturas: `stb_image` para png/jpg e decodificador .dds.
- [ ] Gerar mipmaps via device context (`GenerateMips`).
- [ ] Converter materiais glTF → parâmetros PBR internos da KSGE.
- [ ] Criar sistema de pool/arena de memória para assets (sem fragmentação).
- [ ] Carregar um chunk de teste (modelos variados) e exibir.

### 8.4 Critérios de Aceite

- Modelo de teste com mais de 100k triângulos carrega e renderiza.
- Instancing suportado a partir dos buffers interleaved.
- Texturas com mipmap sem lixo visual.

---

## 9. Fase 5 — Pipeline de Renderização PBR

### 9.1 Objetivo

Implementar renderização fisicamente correta: materiais com albedo, roughness, metallic e normal maps processados no pixel shader, com iluminação por luz pontual/direcional em espaço de mundo.

### 9.2 Entregáveis

- Shaders HLSL SM 5.0: vertex/pixel para malhas PBR.
- Estrutura de material uniforme (albedo, metallic, roughness, normal, ao, emissive).
- IBL básico via skybox/probe hemisférica (fallback até GI completa).
- Skybox cubemap.

### 9.3 Tarefas Detalhadas

- [ ] Escrever `pbr_vs.hlsl`: transformação de malha + normal → world.
- [ ] Escrever `pbr_ps.hlsl`: BRDF Cook-Torrance com GGX.
- [ ] Calcular o termo de Fresnel (Schlick) e Normal Distribution.
- [ ] Incluir geometria shadow term (Smith-GGX).
- [ ] Ler roughness/metallic das texturas e aplicar mapeamento.
- [ ] Aplicar normal map via tangente (TBN) e normalização.
- [ ] Implementar base do IBL: convolução difusa simples + specular fallback.
- [ ] Criar constant buffer de cena: view/proj/cam pos, light dir, sun color.
- [ ] Criar skybox com cubemap e depth escreve só onde não há objetos.
- [ ] Teste de referência: esfera metálica, esfera rough, cubo albedo.

### 9.4 Critérios de Aceite

- Esfera metálica reflete com Fresnel adequado.
- GT 610 mantém 30+ FPS com cena de teste simples em 720p.
- Cores/specular consistentes com referência PBR.

---

## 10. Fase 6 — Pós-Processamento Avançado ✅ CONCLUÍDA

### 10.1 Objetivo

Pipeline pós-fx em fullscreen pass: Volumetric Fog, Bloom físico, Color Grading via LUT e SSAO.

### 10.2 Entregáveis

- [x] RT intermediários (HDR) com formatos suportados por DX11 (RGBA16F/R11G11B10).
- [x] Passes encadeados de downsample/upsample para Bloom.
- [x] Volumetric fog analytical (Ray/Height fog) + scattering.
- [x] SSAO com blurs esféricos/heurísticos.
- [x] LUT 3D de color grading aplicada no final.

### 10.3 Tarefas Detalhadas

- [x] Configurar swapchain e cena em HDR com tone mapping no final.
- [x] Bloom: threshold → downsample 4x → blur radial/bilateral → upsampling → adicionar.
- [x] Volumetric fog: integrar densidade ao longo do raio no frustum em resolução reduzida, com jitter temporal opcional.
- [x] SSAO: gerar noise rotativo, calcular oclusão, aplicar blur bilateral de normal e depth.
- [x] Color grading: carregar `.cube`/`LUT` 32³, amostrar trilinear, aplicar exposição e gamma.
- [x] Combined pass final: fundir SSAO/Diffuse/Specular/Fog/Bloom/Grade.
- [x] Debug views por teclado (0-6) para validar cada pass.

### 10.4 Critérios de Aceite

- [x] Cena com contraste suave, sem banding na GT 610 (validado headless via WARP no CI; correção visual final pendente do usuário no hardware alvo).
- [x] Fog volumétrico com luz solar atravessando (god rays simples).
- [x] LUT trocando o look inteiro da cena instantaneamente.

---

## 11. Fase 7 — Iluminação Global em Software ✅ CONCLUÍDA

### 11.1 Objetivo

Técnicas de screen-space para simular iluminação global realista sem hardware RT: SSGI, SSR e sombras suaves via CSM.

### 11.2 Entregáveis

- [x] G-buffer completo (albedo, normal, depth, roughness, metallic, emissive).
- [x] SSR com ray march em screen space + fresnel.
- [x] SSGI (bounce difuso usando irradiância amostrada do color buffer).
- [x] CSM (cascatas de shadow map com PCF/soft shadows).

### 11.3 Tarefas Detalhadas

- [x] Criar G-buffer com múltiplas render targets (MRT).
- [x] Implementar pass de geometria único para todos os objetos PBR.
- [x] SSR: projetar raio refletido em screen space, ray march com limite, mask por roughness, fade.
- [x] SSGI: reutilizar cor HDR do frame + ray march curta em screen space para bounce difuso; aplicar ao albedo com peso por normal.
- [x] CSM: configurar 3 cascatas (near/mid/far), matrizes ortográficas da luz por cascata, sampler comparison e PCF 5x5.
- [x] Blend de sombras entre cascatas para evitar costura visível.
- [x] Combinar SSGI/SSR com a iluminação direta no light pass.
- [x] Testar cena com luzes múltiplas e ângulos extremos (validação visual final pendente do usuário na GT 610).

### 11.4 Critérios de Aceite

- [x] Reflexos dos objetos na água/especular corretos com rugosidade (validado headless via WARP; visão final na GT 610 pendente do usuário).
- [x] Bounce difuso visível em cantos/oclusões.
- [x] Sombras suaves sem aliasing grosso entre cascatas.
- [ ] GT 610: rendimento ≥ 25 FPS em 720p (a medir no hardware alvo).

---

## 12. Fase 8 — Instanced Rendering e Otimização GT 610

### 12.1 Objetivo

Manter a taxa de quadros estável no hardware de testes (GT 610) usando instancing agressivo e redução de draw calls.

### 12.2 Entregáveis

- Sistema de batching por mesh+material (frustum-culled).
- `DrawIndexedInstanced` para árvores, grama e prédios.
- Culling hierárquico por chunk e por instância.
- Métricas de draw calls e temps de frame (imprimir no console de logs).

### 12.3 Tarefas Detalhadas

- [ ] Agrupar entidades por (mesh, material, caster de sombra) para formar batches.
- [ ] Gerar buffer de instância dinâmica (matrizes de transform, dados por instância).
- [ ] Após frustum cull por chunk, refinar cull por AABB da instância.
- [ ] Substituir draws individuais por `DrawIndexedInstanced` com instâncias por buffer.
- [ ] Reordenar batches para reduzir trocas de estado (state sorting).
- [ ] Implementar sistema de métricas: draw calls, instâncias visíveis, tempo GPU/CPU.
- [ ] Testar cena de estresse: 10k árvores + 20k grama + 5k prédios.
- [ ] Ajustar LOD simples por distância (opcional nesta fase) para reduzir verts.

### 12.4 Critérios de Aceite

- 10k+ instâncias visíveis com < 300 draw calls.
- GT 610 sustenta 30 FPS em bioma denso.
- Métricas acessíveis via painel de logs.

---

## 13. Fase 9 — Mundo Aberto, Chunks e Streaming Assíncrono

### 13.1 Objetivo

Implementar o mundo infinito dividido em chunks de tamanho fixo, carregamento assíncrono baseado em distância do jogador e persistência binária dos chunks modificados.

### 13.2 Entregáveis

- Gerenciador de chunks (registro ativo/descarregado/em IO).
- Thread de streaming assíncrono (lê SSD/VRAM sem travar o frame).
- Serialização binária de chunks (mapas salvos).
- Raio de streaming configurável (slider no editor).

### 13.3 Tarefas Detalhadas

- [ ] Definir tamanho de chunk (ex.: 128m x 128m) e resolução da grid no mundo.
- [ ] Criar `ChunkComponent` no Flecs: local, status (Unloaded→Loading→Loaded→Unloading), path, revision.
- [ ] Criar thread pool de IO com job queue (carregar/descarregar/salvar).
- [ ] Ao mover a câmara: recalcular chord de coordenadas em torno do jogador; enfileirar jobs.
- [ ] Ao concluir IO: instanciar entidades do chunk no mundo Flecs (meshes, luzes, physics proxies).
- [ ] No descarregamento: remover entidades e liberar memória GPU apenas quando não referenciada (refcount).
- [ ] Serializador binário próprio (sem comentários): header mágico, versão, entidades por tipo, componentes em bloco.
- [ ] Salvar apenas chunks com `dirty` flag; `Ctrl+S` grava no disco.
- [ ] Carregar layout do terreno base (heightmap ou procedural simples) para visualização.
- [ ] Testes de teleporte rápido entre biomas para validar streaming sem micro-freezes.

### 13.4 Critérios de Aceite

- Teleporte de +2000m não causa freeze.
- RAM/VRAM crescem apenas com o raio de streaming configurado.
- Salvamento/recarga de chunk preserva transformações e componentes.
- Slider de raio altera comportamento em tempo real.

---

## 14. Fase 10 — Editor Nuklear (Shell da Interface)

### 14.1 Objetivo

Integrar o Nuklear ao pipeline DX11 com tema escuro, fontes vetoriais e a disposição fixa: 4 abas no topo, 2 painéis em baixo, viewport central.

### 14.2 Entregáveis

- Renderizador Nuklear em DX11 (vertex buffer dinâmico, textures atlas).
- Configuração de teto: topo tem abas (tabs), inferior tem dois painéis acoplados.
- Fontes TTF embarcadas (texto + ícones).
- Escala HiDPI.

### 14.3 Tarefas Detalhadas

- [ ] Integrar o cabeçalho `nuklear.h` (single-header).
- [ ] Converter o back-end padrão para um backend DX11 custom: atlas → shader de texto simples + fullscreen textured quad.
- [ ] Criar `nk_context` persistente do editor.
- [ ] Implementar input bridge: GLFW callbacks → Nuklear (teclado/mouse/wheel).
- [ ] Desenhar layout base: viewport central + barra superior (abas) + dock inferior (2 painéis).
- [ ] Criar estruturas `nk_panel` para cada região com configuração de layout.
- [ ] Configurar estilo global: dark theme (ver fase 21).
- [ ] Adicionar frame stat (FPS, ms) no canto.

### 14.4 Critérios de Aceite

- Editor abre com os 3 containers visíveis.
- UI totalmente desenhada pelo Nuklear a 60 FPS sem interferir no render 3D.
- HiDPI sem blur.

---

## 15. Fase 11 — Aba: World Outliner

### 15.1 Objetivo

Listar em tempo real todas as entidades ativas do mundo por chunk, com ícones por tipo, e definir a seleção global da engine ao clicar.

### 15.2 Entregáveis

- Árvore/lista de entidades agrupada por chunk.
- Ícones: cubo (3D), lâmpada (luz), câmera (visualizador).
- Botão "New Entity".
- Comunicação com seleção global da engine.

### 15.3 Tarefas Detalhadas

- [ ] Criar query Flecs que varre entidades dos chunks carregados (filtro por chunk component).
- [ ] Para cada entidade, descobrir tags de tipo (MeshRenderer/Light/Camera) via `flecs::query` e escolher o ícone.
- [ ] Renderizar como `nk_tree` colapsável por chunk no Nuklear.
- [ ] Ao clicar: definir `EditorSelection { entity, gizmo_state }` global.
- [ ] Destacar visualmente a entidade selecionada no outliner.
- [ ] Implementar "New Entity": `world.entity()` com nome default e componentes mínimos (Transform).
- [ ] Atualizar a lista com incremental id sem reconstruir a árvore por completo (buffer ring).

### 15.4 Critérios de Aceite

- Nova entidade aparece instantaneamente.
- Seleção persiste entre abas (partilha um único target global).
- Lista espelha o banco Flecs em tempo real.

---

## 16. Fase 12 — Aba: Inspector

### 16.1 Objetivo

Exibir, modificar e adicionar propriedades da entidade selecionada, lendo direto da RAM alinhada do Flecs, com exposição automática de campos públicos de scripts C#.

### 16.2 Entregáveis

- Widgets Nuklear ligados a components Flecs (Transform, Mesh, Light, Rigidbody, Collider, Audio).
- Botão "Add Component" com menu suspenso.
- Reflexão C# (interop .NET) expondo campos públicos no inspector.
- Edição em tempo real (memória mutável a 60 FPS).

### 16.3 Tarefas Detalhadas

- [ ] Criar registro de componentes conhecidos com metadata de "editor-friendly" (nome, tipo, min/max, step) usado para gerar widgets.
- [ ] Seção Transform: 3 campos position + 3 rotação (graus) + 3 escala.
- [ ] Seção Mesh: campo de caminho gtf e checkbox cast shadows.
- [ ] Seção Light: tipo (ponto/direcional/spot), cor, intensidade, alcance, ângulo.
- [ ] Seção Rigidbody: massa, velocity, uso de gravidade.
- [ ] Seção Collider: forma (box/sphere), tamanho.
- [ ] Seção Audio: arquivo, volume, loop, distancia min/max (3D).
- [ ] Botão "Add Component": popup com lista; ao escolher, anexa componente via `entity.set<T>()`.
- [ ] Ícones por seção: engrenagem (física), alto-falante (áudio), etc.
- [ ] Ponte .NET: para script C# na entidade, invocar API de reflexão para varrer `public` fields e gerar caixas numéricas.
- [ ] Cada alteração de valor → grava direto na memória Flecs (sem cópia desnecessária).

### 16.4 Critérios de Aceite

- Alterar position move o objeto na viewport no mesmo frame.
- Add Component liga física em tempo real.
- Campos públicos do script C# aparecem automaticamente.
- Aba não destrava acima de 1ms no budget.

---

## 17. Fase 13 — Aba: World Streamer

### 17.1 Objetivo

Grade 2D interativa vista de cima da grid de chunks, com estado verde/cinza e slider de raio de streaming.

### 17.2 Entregáveis

- Grid 2D interativa (clique para carregar chunk cinza).
- Legenda de estados (verde=carregado, cinza=descarregado, âmbar=carregando).
- Slider de raio de streaming (metros).
- Câmara 2D pannable sobre o mapa.

### 17.3 Tarefas Detalhadas

- [ ] Converter coordenadas de chunk em células no grid.
- [ ] Colorir células conforme estado (inclusive estado Loading âmbar).
- [ ] Interação: clique em célula cinza → instancia aquele chunk (job de streaming).
- [ ] Slider: `nk_slider_int` → atualiza raio em metros no sistema de streaming (dirty update).
- [ ] Pannable: arrastar com botão direito move a vista 2D; scroll faz zoom.
- [ ] Mostrar posição do jogador/câmara no grid.
- [ ] Indicar direção de streaming preferencial (queada).

### 17.4 Critérios de Aceite

- Clicar em chunk carrega conteúdo no mundo fielmente.
- Slider muda comportamento de streaming sem reiniciar.
- Estados de cor corretos em toda a transição.

---

## 18. Fase 14 — Aba: Visual Scripting Canvas

### 18.1 Objetivo

Canvas infinito de nós interconectáveis que representa a lógica de gameplay; o grafo é a fonte da verdade para gerar código C#.

### 18.2 Entregáveis

- Canvas infinito no Nuklear (pan/zoom, grid).
- Biblioteca de nós: eventos, ações, dados ECS, fluxo.
- Conexões visuais entre ports (in/out) com controle fit.
- Validação de grafo (conectividade, tipos de porta).
- Botão "Compile".

### 18.3 Tarefas Detalhadas

- [ ] Modelo de dados: `NodeGraph { nodes[], edges[], positions }` em RAM (structs de valor, data-oriented).
- [ ] Renderizar nós como Nuklear groups: header colorido por categoria, ports à esquerda/direita.
- [ ] Ports: entrada à esquerda, saída à direita; tipos (Exec, Int, Float, Bool, String, Entity, Vector3).
- [ ] Desenhar conexões como curvas bezier (linhas rasterizadas manualmente ou via quad).
- [ ] Drag from port → highlight de compatibilidade; drop conecta se tipos batem.
- [ ] Navegação: botão médio pan, scroll zoom (âncora para a posição do mouse), F para fit (não confundir com F de focar entidade).
- [ ] Biblioteca de Nós v1:
  - **Eventos:** OnStarted, OnUpdate(Δt), OnCollision(entidadeA, B), OnTrigger, OnKey.
  - **Ações:** ApplyForce, SetVelocity, Teleport, PlaySound, DestroyEntity, SpawnEntity, SetComponent, SetVariable.
  - **Dados ECS:** GetTransform, GetVelocity, GetHealth, GetPosition.
  - **Fluxo:** If/Else, ForLoop, Sequence, Branch.
  - **Math:** Add, Multiply, Lerp, Clamp, Random.
- [ ] Validação: diagrama que avisa nós soltos, loops inválidos, portas desconectadas obrigatórias.
- [ ] Botão "Compile": serializa grafo; dispara fase 19.

### 18.4 Critérios de Aceite

- Grafo de 50+ nós com conexões complexas pan/zoom 60 FPS.
- Deduplicação de conexões e detecção de ciclos.
- Compile gera arquivo .cs válido (ver fase 19).

---

## 19. Fase 15 — Painel: Content Browser

### 19.1 Objetivo

Árvore de diretórios da pasta `/Assets` do estúdio, com scanner assíncrono via `std::filesystem` e drag-and-drop para a viewport/inspector.

### 19.2 Entregáveis

- Scanner assíncrono de pastas (não bloqueia UI).
- Ícones por tipo: pasta, objeto 3D (.gltf/.glb), textura (.png/.dds), script (arquivos de nós).
- Drag-and-drop via GLFW para viewport ou "Add Component".

### 19.3 Tarefas Detalhadas

- [ ] Implementar scanner com `std::filesystem::directory_iterator` em thread de IO.
- [ ] Garantir estilos de arquivos: extensão → ícone + cor.
- [ ] Cache de árvore com versionamento (mtime) para atualização suave.
- [ ] Drag: ao iniciar arrasto de item, guardar asset handle.
- [ ] Drop na viewport: criar entidade com mesh do asset na posição projetada (raycast).
- [ ] Drop no "Add Component" do Inspector: anexar material/mesh no componente da entidade selecionada.
- [ ] Gerir atalhos locais: duplo clique abrir mapas/scripts no canvas.

### 19.4 Critérios de Aceite

- Pastas com milhares de arquivos não travam o editor.
- Soltar um .gltf na viewport cria entidade visível.
- Soltar textura no inspector liga no material selecionado.

---

## 20. Fase 16 — Painel: Output Console

### 20.1 Objetivo

Terminal persistente no canto inferior direito exibindo saídas do sistema, erros de compilação C#, logs .NET e mensagens de debug DX11, com colorização, Clear e filtro instantâneo.

### 20.2 Entregáveis

- Buffer circular de strings no Nuklear.
- Captura de `std::cout`/`std::cerr`.
- Hook de logs do .NET (escrita redirecionada).
- Colorização: branco (info), amarelo (warnings performance), vermelho (erros).
- Botão "Clear" e filtro de pesquisa instantânea.

### 20.3 Tarefas Detalhadas

- [ ] Implementar `LogBuffer` (ring buffer com capac. configurável, bloqueio leve por mutex).
- [ ] Redirecionar `std::cout`/`std::cerr` via rdbuf custom alvo ao LogBuffer.
- [ ] Registrar callback .NET (ITraceListener / console redirect) para logs do CoreCLR.
- [ ] Classificar mensagens por nível com regex/prefixos e aplicar cor.
- [ ] Renderizar lista com compressão automática de linhas duplicadas (counter).
- [ ] Botão Clear → zera buffer mantendo contagem de erros.
- [ ] Filtro: `nk_edit` em texto com busca case-insensitive substring ao vivo.
- [ ] Auto-scroll toggle (seguir fundo por padrão).

### 20.4 Critérios de Aceite

- Fluxo de 10k logs/seg não destrava a UI.
- Erros de script C# aparecem em vermelho com número da linha.
- Filtro em "roug" filtra tudo que contenha a substring.

---

## 21. Fase 17 — Input, Atalhos e Gizmos

### 21.1 Objetivo

Mapear inputs via GLFW para os modos de edição, atalhos globais e gizmos clássicos da indústria.

### 21.2 Entregáveis

- Modos Q/W/E/R com estados visuais + atalhos.
- Gizmo Translate (3 setas), Rotate (3 arcos), Scale (3 linhas+cubos).
- Atalhos Ctrl+Z, Ctrl+S, Ctrl+D, Delete, F.
- Navegação de câmera livre (botão direito + WASD), raycast de seleção e zoom por scroll.

### 21.3 Tarefas Detalhadas

- [ ] Estado `EditorTool { Select, Translate, Rotate, Scale }`.
- [ ] Q/W/E/R alternam estado; toolbar mostra botão ativo (icon highlight).
- [ ] Gizmo Translate: detectar hover em eixo (X vermelho, Y verde, Z azul) via raio vs linha; drag projeta no plano do eixo; escreve na posição Flecs.
- [ ] Gizmo Rotate: 3 arcos; arrasto calcula ângulo delta em relação ao centro; atualiza quaternion.
- [ ] Gizmo Scale: linhas com cubos; arrasto escala uniforme ou por eixo (definir por quadrado selecionado).
- [ ] Picking: LMB → ray × AABB das entidades (custo barato, gizmos por cima).
- [ ] Ctrl+Z: command stack de transformações/componentes (undo/redo) sobre Flecs.
- [ ] Ctrl+S: serializar chunks dirty.
- [ ] Ctrl+D: clonar entidade (novo ID, componentes copiados).
- [ ] Delete: destruir entidade e remover do banco.
- [ ] F: focar a entidade selecionada (move câmara de frente).
- [ ] Câmera: RMB segura + WASD move, mouse rotaciona, scroll dá zoom.
- [ ] Na presença de gizmo ativo: LMB interage com o eixo selecionado em vez de selecionar.

### 21.4 Critérios de Aceite

- Arrastar eixo X move só em X (Z e Y fixos).
- Undo/Redo cobre última transformação.
- Duplicar mantém componentes e gera ID novo.
- Focar posiciona câmara sem colidir dentro de objetos.

---

## 22. Fase 18 — Integração .NET 10 (CoreCLR Hosting)

### 22.1 Objetivo

Embutir o runtime .NET 10 no processo C++ via `nethost` e `hostfxr` para Native Hosting, permitindo executar C# gerado pelo editor.

### 22.2 Entregáveis

- Bootstrap nethost/hostfxr (load_assembly / load runtime).
- Host do CoreCLR com interface C++/C#.
- Exposição de API da engine (delegate table) para C#.
- Logs .NET integrados ao Output Console.

### 22.3 Tarefas Detalhadas

- [ ] Localizar runtime .NET 10 via `nethost.h` (`get_hostfxr_path`).
- [ ] Carregar hostfxr.dll via `LoadLibraryExW`.
- [ ] Inicializar em modo consumo: `hostfxr_initialize_for_dotnet_command_line` (deployment runtime do jogo em pasta local).
- [ ] Obter runtime: `hostfxr_get_runtime_delegate` → `hdt_load_assembly_and_get_function_pointer`.
- [ ] Criar assembly `KSGE.Runtime.dll` com exportações estáticas (UnmanagedCallersOnly).
- [ ] Registrar função de entrada: `Init(EngineHooks)` e `Tick(dt)`.
- [ ] Passar ponteiros de função via `delegate table` para o C# lançar gizmos, criar entidades, aplicar força etc.
- [ ] Implementar `ITraceListener` custom e encaminhar ao LogBuffer.
- [ ] GC/ICUs do runtime configurado para jogo (server GC option testável).
- [ ] Carregar scripts user .cs (compilados previamente) como assemblies adicionais.

### 22.4 Critérios de Aceite

- DLLs C# carregadas e métodos chamados a cada frame.
- Exceção C# → stack trace completa no Console com vermelho.
- Sem vazamentos de host no exit (shutdown limpo).

---

## 23. Fase 19 — Geração de Código C# a partir de Nós

### 23.1 Objetivo

Traduzir o grafo de nós serializado em um arquivo `.cs` válido e compilável que reimplante a lógica visual.

### 23.2 Entregáveis

- Serializador de grafo (JSON binário próprio).
- Backend de geração de código (C# codegen).
- Pipeline de valorização de tipos e escopos.
- Output de erros de compilação no Console.

### 23.3 Tarefas Detalhadas

- [ ] Definir esquema de nó: `id, type, position, params, outputs[], inputs[]`.
- [ ] Ordenar topologicamente o grafo (DFS/Kahn); detectar ciclos.
- [ ] Mapear cada nó para um trecho C# template:
  - OnUpdate → método `void OnUpdate(float dt)`.
  - ApplyForce → `physics.AddForce(entity, vec3)`.
  - GetHealth → leitura de componente.
- [ ] Gerar classes por script: `public partial class GEN_<name> : KSGEScript`.
- [ ] Gerar campos públicos para variáveis nomeadas (vão aparecer no Inspector).
- [ ] Wrap de execução: cada port exec → chamada sequencial com `if`/scopes para Branch.
- [ ] Compilar com Roslyn (via runtime do .NET) no background.
- [ ] Escrever mensagens de erro com correlação de nó (função `// node 42`) — geradas em runtime, não no código da engine.
- [ ] Hot-reload do assembly em Play sem reiniciar editor (bônus).

### 23.4 Critérios de Aceite

- Grafo "OnUpdate → If(PressKey) → ApplyForce → PlaySound" gera C# funcional.
- Compilar no Run diz exatamente qual nó falhou.
- Incremental: mudar nó recompila em < 1s.

---

## 24. Fase 20 — Compilação e Exportação do .exe Final

### 24.1 Objetivo

Gerar o executável nativo standalone .exe que embute editor→jogo, runtime CoreCLR, assets e recursos, otimizado com Native AOT.

### 24.2 Entregáveis

- Export pipeline com passos: grafo → .cs → compile → link engine → bundle.
- Opção "Build/Exportar" na toolbar.
- Validação do .exe gerado (smoke test headless/WARP no CI).

### 24.3 Tarefas Detalhadas

- [ ] Separação de modos: `KSGE.exe --editor` (estúdio) e `KSGE.exe --play <mapa>` (jogo).
- [ ] Empacotar: embed assets como recursos (Win32 resource) ou folder autosuficiente (escolher: folder com DLLs — mais estável).
- [ ] Opção Native AOT do .NET para scripts/assemblies do jogo; mesclar em lib nativa loadable.
- [ ] Passo de remoção de símbolos e otimização para tamanho.
- [ ] Gerar manifest e ícone custom do .exe.
- [ ] Smoke test: rodar o .exe com WARP (no GitHub runner Windows) por 5s e capturar exit code + logs.
- [ ] Assinatura opcional e versionamento do artefato na Release.

### 24.4 Critérios de Aceite

- .exe único ou zip com DLLs roda em máquina limpa sem SDK instalado.
- Jogo com 50 nós de script roda com gameplay estável.
- CI valida o artefato remotamente.

---

## 25. Fase 21 — Tema Escuro Autoral e Ícones Vetoriais

### 25.1 Objetivo

Identidade visual exclusiva: dark mode profissional, minimalista e autoral, com fonte de ícones vetoriais (.ttf) mapeada via código na toolbar.

### 25.2 Entregáveis

- Fontes TTF embarcadas: texto UI + ícones.
- Tabela de glifos mapeada (mover/rotacionar/escalar/play/pause/build).
- Tema de cores custom do Nuklear aplicado globalmente.
- Estilos de nós/abas/árvores coerentes com a marca.

### 25.3 Tarefas Detalhadas

- [ ] Embed TTF (mover, rotacionar, expandir, play, pause, build, cubo, lâmpada, câmera, pasta, imagem, código, engrenagem, alto-falante, etc.).
- [ ] Definir paleta: bg topo `#0D1117`, bg painel `#161B22`, accent `#E448FA`/escolha autoral, texto `#E6EDF3`, seleção `#2F81F7` — até decidir identidade da Kizuri.
- [ ] Aplicar `nk_style` global: cores, bordas, arredondamento, padding, hover.
- [ ] Botões de toolbar: ícone + tooltip; ativo com accent.
- [ ] Tabs customizadas com separadores sutis.
- [ ] Scrollingbars finos (dark) e combobox minimalistas.
- [ ] Validação visual em 720p na GT 610 (sem overhead de sombra/blur na UI).

### 25.4 Critérios de Aceite

- UI 100% coesos (sem contraste aleatório).
- Ícones vetoriais nítidos em qualquer escala (font smoothing).
- Overhead da UI < 0.5 ms na GT 610.

---

## 26. Fase 22 — Polimento, Perf e Testes de Qualidade

### 26.1 Objetivo

Ajustar performance no hardware alvo, testar todos os fluxos do editor, estressar streaming/render e esmagar bugs de latência.

### 26.2 Entregáveis

- Perfilador leve embutido (frame breakdown por pass).
- Suite de testes automatizados (CI) + testes manuais guiados.
- Relatório de performance na matriz GT 610.

### 26.3 Tarefas Detalhadas

- [ ] Instrumentar cada pass do renderer com timestamps GPU (queries).
- [ ] Adicionar overlay de debug no editor (GPU ms, CPU ms, draw calls, triangles, chunks ativos).
- [ ] Testes unitários: matemática, serialização, codegen (snapshot do .cs gerado vs esperado).
- [ ] Testes de integração headless com WARP: criar entidade, modificar, salvar, carregar.
- [ ] Testes de stress: 100k entidades, 30 chunks flying, 10 scripts C# em paralelo.
- [ ] Memória: validar com allocations counters (sem vazamento em ciclos de streaming).
- [ ] Estabilidade: rodar editor 1h + jogar 1h com teleporte contínuo.
- [ ] Regressão visual dos shaders (imagens de referência por pass).

### 26.4 Critérios de Aceite

- Sem crash/leak em testes de 1h.
- Budget por frame: GPU ≤ 15ms, CPU UI ≤ 2ms, streaming no thread não afeta frame.
- CI roda toda a suíte verde.

---

## 27. Fase 23 — Empacotamento e Lançamento (Release)

### 27.1 Objetivo

Produzir a primeira versão pública com pipelina de release 100% automatizada via GitHub.

### 27.2 Entregáveis

- Tag `v1.0.0`.
- Artefato do editor + exporter + runtime.
- Notas de release, changelog, guia rápido.
- Validação do artefato em máquina limpa.

### 27.3 Tarefas Detalhadas

- [ ] Finalizar `release.yml` com build Release assinado/versionado.
- [ ] Gerar zip com: KSGE editor, DLLs .NET, SDK/Assets de exemplo, README rápido.
- [ ] `gh release create` com changelog automático (conventional commits).
- [ ] Documentar requisitos mínimos (GT 610, DX11, Windows).
- [ ] Criar demo de exemplo: mundo aberto com streaming, poucas mecânicas via nós.
- [ ] Publicar e anexar vídeo/gifs.

### 27.4 Critérios de Aceite

- Download do zip → rodar → abrir demo sem instalar nada.
- Release tag com checks verdes.

---

## 28. Roadmap Pós-Versão 1.0 (V2)

- [ ] Skinning/animação de personagens (glTF skin+anim).
- [ ] Sistema de partículas GPU.
- [ ] Terreno procedural com LOD (quadsplat/geometry).
- [ ] PBR extended (clearcoat, iridescência, subsurface).
- [ ] Sistema de física custom ou integrar Jolt (com onboarding).
- [ ] Networking / multiplayer (replicação Flecs).
- [ ] Audio 3D com HRTF básico.
- [ ] Scripting melhorado: debug stepping, breakpoints, variáveis ao vivo.
- [ ] Móvel (Android/Vulkan) — maximizar portabilidade de data-oriented core.
- [ ] Marketplace de assets/chunks.
- [ ] Undo/redo aprimorado com histórico persistido.

---

## 29. Dicionário Técnico e Glossário

- **KSGE (Kizuri Studio Game Engine):** Nome oficial da engine.
- **DX11:** Direct3D 11, API gráfica nativa da engine (Feature Level 11_0).
- **HWND:** Handle nativo da janela Windows.
- **Nuklear:** Biblioteca single-header C de UI imediate-mode.
- **Flecs:** ECS data-oriented utilizado como banco de dados em RAM.
- **fastgltf:** Carregador de .gltf/.glb.
- **DirectXMath / GLM:** Bibliotecas de matemática 3D.
- **PBR:** Physically Based Rendering (albedo, roughness, metallic, normal).
- **SSGI:** Screen Space Global Illumination.
- **SSR:** Screen Space Reflections.
- **CSM:** Cascaded Shadow Maps (sombras suaves).
- **SSAO:** Screen Space Ambient Occlusion.
- **LUT:** Look-Up Table para color grading.
- **Chunk:** Bloco do mundo (ex.: 128m²) gerenciado pelo streamer.
- **Streaming:** Carregamento assíncrono de dados do mundo.
- **Native AOT:** Ahead-of-Time do .NET → código nativo.
- **CoreCLR/nethost/hostfxr:** Componentes do .NET hosting nativo.
- **ECS:** Entity Component System.
- **AABB:** Bounding box alinhado aos eixos (picking).
- **DOD:** Data-Oriented Design.

---

## 30. Definição de Pronto (DoD) Global

Toda Fase só está concluída quando:

- [x] Código no repositório com nenhum comentário no fonte.
- [x] CI verde (build + testes) no GitHub.
- [x] Critérios de aceite da fase verificados.
- [x] Console de logs sem erros críticos relacionados.
- [x] Performance na GT 610 dentro do budget declarado.
- [x] Compila em Release com warnings zerados relevantes.
- [x] Roadmap atualizado, fase marcada como concluída.

---

## 31. Convenções de Código e Estilo

1. **C++ moderno:** C++20/23, autos, RAII, structs value-types, ranges quando claro.
2. **Data-Oriented:** componentes Flecs; agregação de dados; evitar cache misses.
3. **Sem comentários** em nenhum arquivo-fonte (exigência do `OPENCODE.md`).
4. **Nomes:** `camelCase` para funções/variáveis; `PascalCase` tipos; prefixo de módulo (ex.: `dx11_`, `ui_`, `world_`).
5. **HLSL:** Uso de constant buffers pequenos e estáveis; `#include` de common.
6. **C#:** Classes `sealed`, campos públicos para exposição ao inspector, UnmanagedCallersOnly para exports.
7. **Git:** Conventional Commits (`feat:`, `fix:`, `ci:`, `refactor:`); PRs pequenos.
8. **Testes:** Unit tests ao lado do código; integração headless via WARP no CI.

---

## 32. Risco / Gestão de Dependências

| Risco | Impacto | Mitigação |
|---|---|---|
| GT 610 não segura PBR+GI | Alto | Instancing, redução de resolução de passes, LOD, ambient cache |
| DX11 ausente em runner | Médio | Fallback WARP para testes CI |
| .NET hosting fragility | Alto | Host pointer table patchada, logs detalhados, fallback error report |
| Streaming freeze | Alto | IO thread + double buffering, teste de teleporte |
| Nuklear/DX11 integração | Médio | Backend dedicado com atlas e shader simples |
| Asset pipeline lento | Médio | Cache hash + arena de memória |
| CI lento | Baixo | Cache vcpkg/conan e artefatos |

---

## 33. Ambiente de Teste (GT 610) — Matriz de Qualidade

| Cenário | FPS Alvo | Qtd. Instâncias | Obs. |
|---|---|---|---|
| Cena vazia (viewport) | ≥ 60 | 0 | Baseline |
| Bioma denso | ≥ 30 | 10k+ árvores/grama | Instancing obrigatório |
| Streaming teleporte 2000m | sem freeze | — | Max frame spike < 100ms |
| Editor completo + 50 nós | ≥ 30 | — | UI overhead < 2ms |
| Física 100 rigidbodies | ≥ 40 | — | Flecs + dados contíguos |
| Cena PBR + GI completa | ≥ 25 | 5k | SSGI/SSR reduzidos c/ LOD |

---

## 34. Comandos úteis do GitHub CLI

> Build/teste NUNCA localmente. Todos os comandos abaixo são remotos.

- `gh repo create KizuriStudio/KSGE --private --source . --remote origin --push`
- `gh workflow run ci.yml` / `gh run watch`
- `gh run list --limit 10` / `gh run view <id>`
- `gh run download <id>` — baixar artefatos.
- `gh api repos/KizuriStudio/KSGE/actions/workflows` — listar workflows.
- `gh release create v1.0.0 --generate-notes --title "..."`

---

## 35. Checklist de Entrega Final

- [ ] Editor com 4 abas + 2 painéis funcionais.
- [ ] Gizmos Q/W/E/R + atalhos completos.
- [ ] Streaming de entrada com chunks e persistência.
- [ ] Visual Scripting gerando C# funcional.
- [ ] Runtime CoreCLR integrado e recebendo scripts.
- [ ] Renderer PBR + pós + GI com instancing.
- [ ] Tema escuro autoral e ícones vetoriais.
- [ ] Export .exe final validado no CI.
- [ ] Release publicado no GitHub.

---

## 36. Log de Decisões Arquiteturais (ADR)

### ADR-001 — Nome do projeto
- **Status:** Aceito
- **Decisão:** KSGE (Kizuri Studio Game Engine).
- **Contexto:** Nome anterior incorreto corrigido para a identidade da Kizuri Studio.

### ADR-002 — API gráfica
- **Status:** Aceito
- **Decisão:** Direct3D 11 puro, Feature Level 11_0.
- **Motivo:** Performance nativa no Windows + compatibilidade com GT 610 (sem DX12/RTX).

### ADR-003 — UI do editor
- **Status:** Aceito
- **Decisão:** Nuklear (single-header C) integrado ao pipeline DX11.
- **Motivo:** Immediate mode rápido, leve e totamente customizável via código.

### ADR-004 — Data layer
- **Status:** Aceito
- **Decisão:** Flecs como banco de dados de mundo em RAM alinhada.
- **Motivo:** Performance ECS superior e compatível com data-oriented design.

### ADR-005 — Scripting
- **Status:** Aceito
- **Decisão:** CoreCLR do .NET 10 via nethost/hostfxr, C# gerado de nós.
- **Motivo:** Produtividade do C# com hot-reload e AOT para build final.

### ADR-006 — Streaming
- **Status:** Aceito
- **Decisão:** Chunk grid + thread de IO assíncrona.
- **Motivo:** Mundo infinito sem travar a gameplay.

### ADR-007 — CI/CD
- **Status:** Aceito
- **Decisão:** Build 100% via GitHub Actions; proibido build local.
- **Motivo:** Diretriz obrigatória do OPENCODE.md.

### ADR-008 — Geração de código (v2, proposta)
- **Status:** Proposto
- **Decisão:** Codegen por templates por tipo de nó com correlação de erro.
- **Motivo:** Manter gráficos de 50+ nós compilando rápido.

---

> **Fim do ROADMAP.**  
> Este documento é a fonte única de planejamento da KSGE. Alterações arquiteturais devem entrar aqui e no `OPENCODE.md`.