# Plano do skin changer interno de CS2 em laboratório

**Objetivo** — Validar uma troca cosmética em primeira pessoa dentro do cliente, em uma sessão local de teste, com aparência reproduzível e retorno ao acabamento original.

**Redução de risco de banimento** — Executar o protótipo em um ambiente dedicado com a rede externa bloqueada antes da abertura do jogo. O bloqueio fica fora do protótipo e permanece ativo durante toda a sessão. Os testes usam um mapa local e registram o isolamento como condição de execução. A reconstrução de 15 módulos clássicos mostra o comportamento das amostras estudadas. A política de detecção da versão atual do CS2 segue desconhecida. [Reconstrução do VAC](https://github.com/sapdragon/vac) · [Suporte Steam sobre o VAC](https://help.steampowered.com/en/faqs/view/571A-97DA-70E9-FF74)

**Primeiro recorte** — Uma arma e dois acabamentos compatíveis. Escolher um com padrão variável e outro com desgaste bem visível. Usar armas de fogo nesta etapa e avaliar facas e luvas após validar o ciclo de aplicação e restauração.

## 1. Validar o ambiente

- [x] Registrar a build do CS2, a máquina, a GPU, a resolução e os parâmetros gráficos salvos.
- [ ] Com a rede externa bloqueada, confirmar que a build abre um mapa local e que conexões externas estão indisponíveis. Fazer essa checagem com o cliente limpo, antes de carregar o protótipo.
- [x] Definir como a sessão será encerrada antes de reconectar o ambiente. Fechar o jogo e descartar o estado de teste ou restaurar a imagem limpa.

**Aceite** — O mapa local abre com a rede externa indisponível. Se a build exigir conexão para abrir a sessão, o teste do módulo fica suspenso nessa build.

## 2. Criar a referência visual

- [ ] Capturar o acabamento original na animação de inspeção e com a arma em mãos.
- [ ] Para acabamentos próprios, gerar referências com **Inspect** e **Preview** do Workshop Item Editor. Usar a inspeção nativa quando a referência for um item existente e estiver disponível.
- [ ] Fixar câmera, pose, resolução e luz para a comparação. Capturar três valores de desgaste dentro da faixa do acabamento e três seeds no acabamento com padrão variável. Registrar os valores ao lado das capturas. [Guia de acabamentos da Valve](https://www.counter-strike.net/workshop/workshopfinishes)

**Aceite** — Cada captura identifica arma, acabamento, seed, desgaste, build e condições de captura. Comparações em cenas diferentes ficam limitadas à geometria, ao padrão e à distribuição do desgaste.

## 3. Implementar a troca e a restauração

- [x] Representar o preset com `weapon_definition_id`, `paint_kit_id`, `seed` e `wear`. Validar a compatibilidade do kit com a arma e as faixas fornecidas pelo acabamento. A validação é independente do jogo. A integração com o catálogo real e a aplicação ao item permanecem pendentes.
- [ ] Aplicar o preset à arma local em um ponto de atualização do jogo. Resolver a entidade atual quando ela for recriada e atualizar o material quando o preset ou a entidade mudar. A seleção persiste por ID de item e acabamento. [Referência do pipeline cosmético](https://github.com/ZhaLuc/SkinChangerCS2/blob/main/docs/skin_pipeline.md)
- [ ] Salvar o estado original da entidade antes da primeira alteração. Restaurá-lo no reset ou no encerramento enquanto ela existir. Em caso de descarte da entidade, liberar a referência e resolver a próxima a partir do estado atual do jogo.
- [ ] Iniciar com a função desligada. Se a build, a sessão local ou os dados do item não puderem ser validados, conservar o visual original e registrar o motivo.

**Aceite** — Selecionar cada preset altera o visual da arma esperada. Repetir a mesma seleção preserva o resultado. Uma combinação inválida é recusada antes de modificar a arma. Reset e encerramento restauram o acabamento original na entidade ativa.

## 4. Rodar a bateria de qualidade

| Teste | Critério inicial de aceite |
| --- | --- |
| Fidelidade | Com arma, seed e desgaste equivalentes, o padrão, a orientação, as áreas desgastadas e a resposta do material correspondem à referência em inspeção e em mãos. |
| Estabilidade | 30 trocas consecutivas, troca de arma, respawn, recarga do mapa e um reinício completo sem travamento, material incorreto ou alteração residual após o reset. |
| Latência | Medir da confirmação do preset até o primeiro quadro com o material correto. Meta inicial de até 500 ms com os recursos já carregados. |
| Desempenho | Na mesma cena e máquina, comparar três execuções com o recurso desligado e três com o acabamento aplicado. Meta inicial de até 5% de aumento no p95 do tempo de quadro em regime estável. |
| Isolamento | Registrar rede externa indisponível e mapa local ativo no início da sessão. Encerrar o processo de teste antes de restabelecer a conexão. |

## Retomada

### Verificado em 12/11/2026

- Build Steam `26598758`, Windows `22H2` (`19045`), Radeon RX 7600, driver `32.0.23033.1002`. O monitor principal está em `1920×1080`. O CS2 tem `1280×960` em janela no arquivo de vídeo salvo, com `cpu_level=2`, `gpu_mem_level=3`, `gpu_level=3` e `videocfg_texture_detail=0`. O preset efetivo ainda precisa ser conferido na sessão.
- Os arquivos de Ancient e dos mapas de prévia de Ancient, Dust II e Inferno estão presentes. A abertura do mapa e a disponibilidade do Item Editor ainda não foram confirmadas em execução.
- Ethernet e Radmin VPN estavam ativos. A abertura do mapa sem rede fica pendente até haver um ambiente isolado. O CS2 permaneceu fechado durante a checagem.
- `src/preset.hpp` contém o modelo de preset e a validação. `tests/preset_test.cpp` passou 11 verificações em MSVC x64, com IDs e faixas de teste. Para repetir em um terminal **x64 Native Tools** na raiz do projeto, execute `cl /nologo /std:c++20 /EHsc /W4 /WX /Isrc tests\preset_test.cpp` e, em seguida, `preset_test.exe`.

### Bancada de skins

- `ui/index.html` abre a bancada local Acervo no navegador. A tela combina catálogo, imagem de referência e editor de preset. Busca por nome ou ID, ampliação, digitação exata do desgaste, seed e presets por acabamento funcionam com arquivos locais.
- A coleção atual tem 26 acabamentos, incluindo os seis da primeira versão e os 20 de alto valor para AK-47, M4A1-S, M4A4, Desert Eagle, USP-S, Glock-18, facas e luvas. A bancada abre com Case Hardened selecionada. Os limites de desgaste e as imagens de referência vieram do [catálogo CSGO-API](https://github.com/ByMykel/CSGO-API).
- A prévia 2D simula desgaste para todo o catálogo e seed para acabamentos com variação de padrão implementada. **Ver original** exibe a imagem de inventário. A simulação ainda precisa ser comparada com a renderização oficial no CS2.
- A bancada usa os mesmos nomes de campos do validador C++. A persistência fica no navegador. O botão de exportação gera um `.cfg` compatível com o carregador de configurações do módulo interno. A aplicação no cliente e a avaliação de fidelidade visual continuam pendentes.
- A verificação no Edge passou pela busca, ampliação, limites de desgaste, gravação, restauração e recarga dos presets. Foram conferidos foco de teclado, movimento reduzido e layout a 375, 768, 1024 e 1440 px.

### Módulo interno

- `internal/` deriva de [SkinChangerCS2](https://github.com/ZhaLuc/SkinChangerCS2/tree/d4d4951). O projeto compilou em Release x64 após repor o cabeçalho de configuração ausente. O menu interno usa a identidade visual do Acervo e o carregamento de perfis do projeto de origem.
- `ui/profile-export.js` gera os IDs estáveis que `internal/sdk/config_system/config_system.cpp` traduz para o catálogo interno. As dez verificações de exportação cobrem armas, facas e luvas. `internal/lab-load.ps1` bloqueou o carregamento no computador com interfaces de rede ativas.

Próximo passo — Seguir o [tutorial de teste](../../testar-acervo-interno.md) para validar Ancient em ambiente isolado, registrar a arma original e conferir a Case Hardened após carregar o perfil.
