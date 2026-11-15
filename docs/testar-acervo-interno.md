# Testar o módulo nativo legado do Hy3x no CS2

> **Documento de laboratório legado.** Estas instruções descrevem somente o módulo nativo separado em `internal/`, que modifica estado do cliente e exige sessão local `-insecure`. Elas não descrevem o Hy3x Acervo Companion. A bancada atual exporta JSON portátil de coleção e não gera mais `.cfg` para o módulo nativo.

O painel Hy3x by brocco abre no mapa local. A aba **Coleção** permite escolher, visualizar e aplicar os acabamentos. O painel no navegador continua disponível para exportar perfis.

## Comece aqui

- **Pasta do projeto** — `C:\Users\marco\Downloads\cracked`. É onde está o `Acervo-Laboratorio.exe`.
- **Pasta do perfil** — `C:\vc\configs`. É onde você coloca o arquivo `.cfg`.
- **Pacote validado** — extraia o ZIP Windows x64 inteiro e abra o iniciador dessa pasta. O [guia de build](gerar-versao-acervo.md) descreve o manifesto de versão e o runtime necessário.

Para abrir qualquer uma dessas pastas, pressione **Windows + E**, clique na barra de endereço do Explorador de Arquivos, cole o caminho e pressione **Enter**.

## Escolher o acabamento dentro do jogo

Em **Coleção**, clique no cartão de um modelo para abrir o inventário de acabamentos. Clique na imagem do acabamento para ajustar seed e desgaste. Para testar, prepare Ancient como descrito abaixo e abra `Acervo-Laboratorio.exe`.

O painel usa o tema preto e roxo com a opção **Animação RGB**. Para mover a janela, clique e arraste pelo título **Hy3x by brocco**, à esquerda das abas. As imagens vêm do inventário da build instalada `26598758`. Para ver o efeito exato do padrão e do desgaste, inspecione o item após aplicar.

1. Equipe a AK-47 no mapa local. Pressione **Delete** se o painel estiver fechado.
2. Em **Coleção → Rifles → AK-47 → Case Hardened**, abra **Padrões de colecionador** e escolha **Blue Gem #661 (Scar)**. Clique em **Aplicar no jogo**, feche o painel e inspecione o metal da arma.
3. Abra **Facas → Butterfly Knife → Doppler - Phase 2** e escolha **Pink Galaxy #339**. Repita com **Doppler - Phase 4 → Max Blue #763** para comparar. A lista mostra os acabamentos compatíveis com cada um dos 20 modelos.
4. Em **Luvas → Sport Gloves**, aplique **Hedge Maze** (verde), depois **Pandora’s Box** (roxa) ou **Vice** (rosa e azul). Observe quanto tempo leva para a cor mudar e confira se aparece uma camada duplicada durante o freeze e logo após o início do próximo round.
5. **Repintura solicitada** indica que o jogo reconheceu o acabamento e recebeu o pedido de atualização. Se o visual continuar igual, anote o item, a mensagem abaixo de **Aplicar no jogo** e se o problema ocorreu durante o freeze.

## Salvar um kit

1. Abra o acabamento, ajuste a seed e o desgaste e clique em **Adicionar ao kit**.
2. Repita com outras armas, uma faca e um par de luvas. A última escolha substitui a anterior para a mesma arma, faca ou par de luvas.
3. Abra **Kits**, digite um nome e clique em **Salvar kit**. Depois, abra o kit salvo e clique em **Aplicar kit**. O Hy3x guarda a seleção em `C:\vc\configs\kits.json`.

Os pares de modelo e acabamento do catálogo foram conferidos pelos arquivos de inventário instalados. [Guia de Blue Gems da AK-47](https://steamcommunity.com/sharedfiles/filedetails/?id=2941982575), [guia de Blue Gems de facas](https://steamcommunity.com/sharedfiles/filedetails/?id=1889871377), [Butterfly Doppler Phase 2](https://steamcommunity.com/sharedfiles/filedetails/?id=3260663518), [Butterfly Doppler Phase 4](https://steamcommunity.com/sharedfiles/filedetails/?id=3246704692).

## 1. Colocar um perfil na pasta (opcional)

1. Copie o [perfil pronto da AK-47 Case Hardened](../acervo-case-hardened.cfg) para `C:\vc\configs\` no ambiente de laboratório. Ele usa seed `857` e desgaste `0,24`. Se já houver um arquivo de mesmo nome, preserve-o e dê outro nome ao perfil de teste, como `acervo-case-hardened-857.cfg`.
2. Para este teste legado, use somente um perfil de laboratório previamente preparado, como `acervo-case-hardened.cfg`. A bancada atual em `ui/index.html` exporta JSON portátil e não produz arquivos consumidos pelo módulo nativo.

## 2. Abrir o mapa no laboratório

1. No ambiente dedicado, feche o CS2, prepare o modo offline da Steam e desconecte a rede externa. Ethernet, Wi-Fi, VPN e interfaces virtuais com endereço IP podem impedir a checagem do carregador. O comando abaixo informa quais ainda estão ativas. [Modo offline da Steam](https://help.steampowered.com/en/faqs/view/0E18-319B-E34B-B2C8)
2. Na biblioteca da Steam, clique com o botão direito em **Counter-Strike 2 → Propriedades → Geral**. No campo **Opções de inicialização**, acrescente `-insecure -allow_third_party_software -console`. [Modo confiável do CS2](https://help.steampowered.com/en/faqs/view/09A0-4879-4353-EF95)
3. Abra o CS2. Na janela de **console dentro do jogo**, digite `map de_ancient` e pressione **Enter**. Se o console não aparecer, ative o console de desenvolvedor nas configurações do jogo. [Console do CS2](https://help.steampowered.com/en/faqs/view/4700-D10E-26BE-DDDD)
4. Equipe a AK-47 e capture a aparência original em mãos e na animação de inspeção. Se o mapa local não abrir, registre a mensagem e encerre a sessão de teste. Mantenha a rede desconectada até fechar o jogo.

## 3. Carregar e conferir

1. Com Ancient aberto, abra a pasta extraída de uma versão e dê duplo clique em **Acervo-Laboratorio.exe**. Clique em **Carregar painel**. A janela mostra a pré-checagem de rede, a build do jogo, a verificação da DLL e, quando o desenho estiver disponível, **Painel pronto**. Se houver falha, use **Copiar diagnóstico** para copiar a mensagem e o caminho do log quando estiver disponível.
2. Use **Delete** se o menu estiver fechado. Para testar um `.cfg`, vá a **Perfis**, selecione `acervo-case-hardened` (ou o nome que você escolheu) e clique em **Carregar**. Use **Atualizar** se o arquivo foi copiado depois da abertura da aba.
3. Inspecione a AK-47 sob a mesma luz e capture o resultado. Em seguida exporte outro perfil com seed `1`, carregue o segundo arquivo e compare o padrão. Renomeie cada arquivo no Explorador para distinguir as duas seeds. Repita com outro desgaste, mantendo a seed fixa.
4. Troque de arma, renasça e recarregue Ancient. Confira se a skin volta na AK certa. Em sessões seguintes, repita com **Karambit Gamma Doppler** e **Sport Gloves Pandora’s Box**, cada um com seu perfil.
5. Para testar a parada do painel, volte ao jogo e pressione **End**. Confira se o painel desaparece e o jogo continua aberto. Feche o CS2 ao concluir e confira o visual original após abrir uma sessão limpa. A restauração de todos os atributos em uma arma já alterada ainda precisa ser validada. Feche a sessão local antes de reconectar o ambiente.

## O que enviar de volta

- Build do CS2, mapa, acabamento, seed e desgaste.
- Capturas antes e depois na mesma luz. Se puder, inclua a animação de inspeção.
- Resultado de cada etapa, desde **CS.dll loaded** até a troca após o respawn. Use **Copiar diagnóstico** para incluir o número da tentativa, as etapas e a mensagem da janela.

Os binários compilados ficam em `internal/x64/Release/`. A compilação e a geração do perfil foram verificadas. Compatibilidade em execução, fidelidade, restauração imediata e desempenho ainda exigem teste no jogo.

## Observar os atributos de um item

Use o pacote identificado por **observe**. Abra o mapa local, carregue o painel e equipe a arma que quer observar. Essa edição registra a definição do item, a contagem de atributos e a presença de dados no log de sessão, em linhas `item_observed`. Ela desativa a aplicação de acabamentos a armas, facas e luvas. O log identifica a edição com `item_observation_mode`.

Faça uma sessão com uma arma padrão e outra com um item de inventário já disponível no ambiente local. Se esse item não estiver disponível offline, registre esse resultado. Para distinguir as amostras, informe qual arma equipou em cada sessão. Os dados observados servem para comparar estados. A capacidade do vetor e quem libera o bloco continuam em investigação.

### Se o jogo fechou logo após carregar

Na build `26598758`, o primeiro registro de falha apontou para a leitura do catálogo de itens. O registro de 13/11/2026 às 19h05 apontou para a leitura dos comandos do jogador. O módulo agora usa a função do catálogo identificada no binário dessa build e deixou de interceptar comandos do jogador. Houve também uma queda às 19h02 no driver AMD em uma sessão sem `CS.dll` carregada. Para repetir o teste, abra Ancient em uma sessão local isolada e use o mesmo `Acervo-Laboratorio.exe` da pasta do projeto. Aguarde o menu aparecer antes de escolher o perfil. Se o jogo fechar de novo, anote a hora e se chegou a ver o menu.

## PowerShell opcional

Para verificar só a rede antes de abrir o jogo, pressione **Windows**, digite **PowerShell** e abra o aplicativo. Cole a linha abaixo e pressione **Enter**. O caminho é completo, então você pode rodar de qualquer pasta.

```powershell
& "C:\Users\marco\Downloads\cracked\internal\lab-load.ps1" -CheckOnly
```

A mensagem **Nenhuma interface externa com endereço IP foi detectada nesta checagem** confirma o resultado pontual. A carga também exige a opção `-insecure` no processo CS2.

## Medir no mapa local

Com o jogo já aberto no mapa local e o pacote extraído, abra o PowerShell na pasta do pacote:

```powershell
.\internal\lab-load.ps1 -MeasureFrames
```

Na mesma cena, deixe o painel fechado e aberto por tempo suficiente para coletar três blocos de 300 quadros por estado. Use o log indicado pelo iniciador, em `%LOCALAPPDATA%\Hy3x`. Cada `frame_sample` informa p95 e p99 em milissegundos. Os eventos `first_frame`, `panel_open` e `panel_closed` registram a sequência. Compare com a mesma resolução, mapa, posição e configuração gráfica.

Para aceitar a troca visual, observe a arma em mãos e em inspeção. Os ícones do inventário e o feed de eliminações passam a seguir a atualização normal do jogo.
