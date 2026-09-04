# Diretivas do Projeto: KSGE (Kizuri Studio Game Engine)

## 📌 Contexto Geral
Instruções para o OpenCode CLI. O projeto KSGE (Kizuri Studio) é uma micro-engine 3D genérica, exclusiva e de alta performance desenvolvida em C++ moderno para jogos de mundo aberto massivo com streaming. A engine utiliza APIs gráficas nativas do Windows para máxima performance, possui um editor visual baseado em Nuklear (Immediate Mode) com suporte a Scripting Visual Node-Based e realiza a compilação e exportação do jogo final em um arquivo executável nativo (.exe) independente.

## 🛠️ Stack Tecnológica e Bibliotecas Utilizadas
A IA deve utilizar estritamente as seguintes bibliotecas para construir o ecossistema da KSGE (Kizuri Studio):
1. **Gráficos (API Nativa Otimizada):** Direct3D 11 (DX11) puro restrito ao Feature Level 11_0 através do Windows SDK.
2. **Janela e Input:** `GLFW` ou `SDL2` (Configurado para expor o ponteiro nativo da janela `HWND` para o DX11).
3. **Editor Visual (Immediate Mode Superior):** `Nuklear` (Biblioteca single-header em C integrada ao pipeline do DX11).
4. **Arquitetura Core:** `Flecs` (A biblioteca de ECS em C++ mais rápida do mercado para gerenciar dados na memória).
5. **Matemática 3D:** `DirectXMath` ou `GLM` (Alinhados para alta performance).
6. **Carregamento de Modelos 3D:** `fastgltf` (Para carregar os modelos .gltf dos chunks do mundo aberto).
7. **Sistema de Scripting (Linguagem do Jogo):** Embutir o runtime do **.NET 10 (CoreCLR)** através das APIs `nethost` e `hostfxr` para Native Hosting, permitindo a execução de códigos em **C#** gerados automaticamente a partir do Editor de Scripting Visual.

## 🎨 Especificações do Pipeline Gráfico NATIVO (HLSL/DX11 - Max Quality)
O renderizador deve alcançar o fotorrealismo por meio de algoritmos puros via software escritos em Shaders HLSL (Shader Model 5.0), contendo:
1. **Renderização PBR (Physically Based Rendering) Avançada:** Cálculo de materiais fisicamente corretos processando Albedo, Roughness, Metallic e Normal Maps via pixel shader tradicional.
2. **Software Ray Tracing & SSGI:** Uso de técnicas de espaço de tela (Screen Space Global Illumination), Screen Space Reflections (SSR) e sombras suaves por mapa de sombras em cascata (CSM) para simular iluminação realista sem depender de hardware RTX/DX12.
3. **Pós-Processamento Avançado:** Shaders focados em atmosfera de ponta, contendo Volumetric Fog (névoa volumétrica simulada), Bloom baseado em física, Color Grading via LUT e Ambient Occlusion (SSAO).
4. **Otimizações Críticas para GT 610:** Uso obrigatório de Instanced Rendering (DrawIndexedInstanced) para agrupar árvores, grama e prédios do mundo aberto, reduzindo as Draw Calls da CPU para manter a taxa de quadros estável no hardware de testes.

## 🖥️ Especificação Detalhada dos Painéis do Editor (Interface Nuklear)
O editor visual em Immediate Mode deve renderizar de forma nativa e limpa quatro abas centrais no topo da tela e dois painéis organizadores na parte inferior da interface, cujo comportamento técnico está lacrado abaixo:

### 🗂️ Abas Superiores Principais
1. **Aba: Hierarquia do Mundo (World Outliner)**
   - **O que faz:** Lista em tempo real todas as Entidades com ID ativo presentes no mundo. Exibe um ícone vetorial de "Cubo" para objetos 3D, "Lâmpada" para luzes e "Câmera" para visualizadores.
   - **Mecânica Interna:** Deve varrer o banco de dados do `Flecs` e exibir apenas as entidades contidas nos chunks (blocos) carregados na memória de vídeo. Ao clicar em uma entidade, ela é definida como o alvo global de seleção da engine. Possui um botão "New Entity" que injeta um novo ID vazio diretamente no banco do Flecs.

2. **Aba: Inspetor de Propriedades (Inspector)**
   - **O que faz:** Exibe, modifica e adiciona propriedades à entidade que foi selecionada na Hierarquia. Cada seção/componente possui um ícone identificador (Engrenagem para física, Alto-falante para áudio).
   - **Mecânica Interna:** Lê os componentes da entidade direto da RAM alinhada do Flecs C++ e renderiza caixas de texto/números do Nuklear. Alterar qualquer valor aqui modifica a memória em tempo real a 60 FPS.
   - **Botão "Add Component":** Abre um menu suspenso permitindo anexar dinamicamente componentes de física (Rigidbody/Collider), áudio 3D ou ligar um script em C# (Visual Scripting) gerado no ecossistema.
   - **Exposição C# (Interop):** Se a entidade possuir um script C# atrelado, a ponte do .NET 10 deve varrer os campos públicos desse script (como vida, velocidade, dano) e desenhá-los de forma automática no Inspetor com caixas de ajuste numérico.

3. **Aba: Gerenciador de Chunks (World Streamer)**
   - **O que faz:** Controla a lógica de carregamento assíncrono do mapa infinito da engine.
   - **Mecânica Interna:** Exibe uma grade 2D interativa vista de cima representando os blocos de mundo aberto. Blocos carregados no SSD/VRAM ficam verdes; blocos descarregados ficam cinzas.
   - **Ação:** Permite ao usuário clicar em um quadrante cinza para instanciar aquele pedaço do mapa e começar a colocar objetos nele. Também expõe um slider numérico para definir o raio de distância (em metros) que o algoritmo de streaming em C++ usará para ler dados em segundo plano enquanto o jogador se move.

4. **Aba: Editor de Nós (Visual Scripting Canvas)**
   - **O que faz:** Cria a lógica comportamental e mecânicas de gameplay do jogo sem digitar código textual.
   - **Mecânica Interna:** Renderiza un canvas infinito em Immediate Mode com suporte ao desenho de nós gráficos interconectáveis (Nodes). Cada nó representa um evento (Ao Iniciar, Ao Colidir, A Cada Frame), uma ação (Aplicar Força, Tocar Som) ou um dado do ECS.
   - **Botão "Compile":** Ao ser clicado, o Core em C++ faz a leitura sequencial do grafo de nós do Nuklear, traduz essa lógica visual em um arquivo de código textual .cs (C#) válido e invoca em background o compilador do .NET 10 (CoreCLR) via Native AOT para gerar o executável final .exe otimizado e independente.

### 📥 Painéis Inferiores de Suporte ao Estúdio
5. **Painel: Gerenciador de Arquivos (Content Browser)**
   - **O que faz:** Exibe a árvore de diretórios local da pasta /Assets do projeto do estúdio em uma janela acoplada na parte inferior esquerda.
   - **Mecânica Interna:** Usa a biblioteca std::filesystem do C++ moderno para varrer pastas de forma assíncrona. Exibe pastas (Ícone de Pasta), arquivos 3D (Ícone de Objeto 3D para .gltf/.glb), texturas (Ícone de Imagem para .png/.dds) e scripts (Ícone de Código para arquivos de nós).
   - **Ação:** Permite arrastar os itens da pasta (drag-and-drop via GLFW) direto para a Viewport 3D ou para o botão "Add Component" do Inspetor para carregar o asset na entidade selecionada no Flecs.

6. **Painel: Terminal de Logs (Output Console)**
   - **O que faz:** Uma janela persistente na parte inferior direita que exibe todas as saídas de texto do sistema, erros de compilação C#, logs dos scripts e mensagens de debug do Direct3D 11 em tempo real.
   - **Mecânica Interna:** Captura as saídas padrão std::cout e intercepta logs do .NET 10 jogando os textos em um buffer circular de strings do Nuklear. Possui código de colorização automática de mensagens (Texto Branco para informações comuns, Amarelo para avisos de performance da engine e Vermelho crítico para erros gráficos ou quebras de script).
   - **Ação:** Possui os botões "Clear" (Limpar buffer) e um filtro de pesquisa em texto instantâneo para encontrar erros específicos no mundo aberto.

## ⌨️ Controle de Input, Atalhos e Ferramentas (Gizmos)
O Core da engine deve mapear os inputs através da GLFW/SDL2 para alternar entre os estados de edição na Viewport 3D usando os padrões clássicos da indústria:

### 1. Modos de Manipulação Espacial (Ferramentas / Gizmos)
- **`Q` - Modo Seleção (Select):** Desativa os eixos visuais. O clique do mouse apenas seleciona a entidade. Exibe o ícone de Seta/Cursor.
- **`W` - Modo Mover (Translate):** Renderiza o Gizmo de 3 setas (Eixos X-Vermelho, Y-Verde, Z-Azul). Arrastar uma seta move o objeto exclusivamente naquele eixo. Exibe o ícone de Cruz de Quatro Setas.
- **`E` - Modo Rotacionar (Rotate):** Renderiza 3 arcos circulares coloridos ao redor do objeto. Arrastar os arcos rotaciona a entidade nos eixos correspondentes. Exibe o ícone de Setas Circulares.
- **`R` - Modo Escalar (Scale):** Renderiza 3 linhas com cubos nas pontas. Arrastar altera o tamanho/escala tridimensional do modelo. Exibe o ícone de Quadrado se Expandindo.

### 2. Comandos Globais e Atalhos de Teclado
- **`Ctrl + Z`:** Desfaz a última alteração de componente/transformação feita no banco do Flecs.
- **`Ctrl + S`:** Salva o estado atual dos chunks modificados em arquivos binários na pasta de mapas (Ícone de Disquete).
- **`Ctrl + D`:** Duplica a entidade selecionada, clonando seus componentes e gerando um novo ID no ECS.
- **`Delete`:** Destrói a entidade selecionada e remove o ID do banco do Flecs imediatamente (Ícone de Lixeira).
- **`F` - Focar (Focus):** Teletransporta a câmera do editor diretamente para ficar de frente com a entidade selecionada (Ícone de Mira/Alvo).

### 3. Navegação da Câmera na Viewport 3D
- **`Botão Direito do Mouse (Segurar) + W, A, S, D`:** Ativa o modo câmera livre estilo avião (Fly Camera). O mouse controla a rotação da câmera (Olhar) e as teclas movem a câmera pelo mundo aberto.
- **`Botão Esquerdo do Mouse`:** Lança um raio matemático tridimensional a partir do cursor na tela para o mundo 3D (Raycasting). Se o raio colidir com a caixa de colisão (AABB) de uma entidade, ela é selecionada. Se houver um Gizmo ativo (W, E, R), o clique interage diretamente com o eixo selecionado.
- **`Scroll do Mouse (Roda)`:** Dá zoom aproximando ou afastando a câmera do foco central.

## 🎨 Sistema de Ícones Vetoriais da Interface
Para garantir exclusividade visual e manter a engine leve para a GPU de testes (GT 610), o editor deve carregar uma fonte de ícones vetoriais em formato .ttf (como Font Awesome / Material Icons) integrada nativamente ao Nuklear. Os seguintes glifos devem ser mapeados via código para compor a barra de ferramentas superior fixa:
- 🛠️ **Barra de Ferramentas Fixa (Superior):**
  - Botão Mover: Ícone de Cruz de Setas
  - Botão Rotacionar: Ícone de Setas em Círculo
  - Botão Escalar: Ícone de Vetor de Expansão
  - Botão Play/Testar: Ícone de Triângulo/Reproduzir (Inicia a simulação do jogo em tempo real na janela).
  - Botão Pause: Ícone de Duas Barras Verticais (Congela os sistemas do Flecs).
  - Botão Build/Exportar: Ícone de Caixa/Pacote (Dispara a compilação final do .exe).

## 🚨 Diretrizes Obrigatórias de Desenvolvimento e Automação
1. **Código Limpo Sem Comentários:** Todo código gerado deve ser estritamente limpo e autoexplicativo. É proibida a inclusão de qualquer tipo de comentário, notas ou documentações internas dentro dos arquivos de código fonte.
2. **Build 100% via GitHub (Comando gh):** Nenhuma compilação, build ou teste deve ser feito localmente na máquina do usuário. Como a ferramenta gh (GitHub CLI) já está autenticada e configurada no terminal, a IA deve criar os arquivos do GitHub Actions (.github/workflows/) e usar comandos remotos do GitHub para buildar, testar e verificar erros diretamente nos servidores do GitHub.
3. **Códigos de Ponta AAA:** Todo código C++ gerado deve aplicar padrões de engenharia sênior (Data-Oriented Design). Proibido o uso de alocações dinâmicas de memória desnecessárias no loop de repetição principal. O Nuklear deve ser integrado de forma limpa ao pipeline de desenho do DX11 e totalmente customizado via código para usar um tema escuro (dark mode) profissional, minimalista e autoral, garantindo a exclusividade visual total do editor da KSGE (Kizuri Studio).
