# Infraestrutura do loader Hy3x

## Primeira entrega

Gerar um ZIP Windows x64 que abre uma janela nativa, verifica o pacote e a sessão local e explica o resultado de cada tentativa. O fluxo começa com os executáveis atuais e os eventos que o módulo já grava.

## Encerramento automático

Não encontrei na documentação pública da Valve um sinal prévio que permita acionar um encerramento antes de uma detecção VAC. Fechar o jogo não reverte o que o sistema já tiver detectado. A compatibilidade com matchmaking oficial permanece sem validação e fica fora dos critérios de aceite desta entrega. [Suporte da Steam sobre VAC](https://help.steampowered.com/en/faqs/view/647C-5CC1-7EA9-3C29) · [Modo Confiável do CS2](https://help.steampowered.com/en/faqs/view/09A0-4879-4353-EF95)

O mecanismo de parada da primeira versão responde a condições que o laboratório consegue verificar.

| Condição | Ação |
| --- | --- |
| Pacote inválido, build incompatível ou pré-checagem de laboratório reprovada | Bloquear a carga antes de iniciar o executável auxiliar e mostrar o motivo. |
| Processo encerrou durante a carga ou antes do primeiro quadro | Encerrar a tentativa e manter o diagnóstico disponível. |
| Tempo limite de carga excedido | Marcar o resultado como indeterminado e bloquear nova carga nessa instância até confirmar o estado ou reiniciar o jogo. A operação pode continuar após o retorno do carregador. |
| Operador solicita parada após a inicialização | Parar novas aplicações, liberar os recursos da sessão e confirmar a desmontagem. Se a confirmação não chegar, pedir que o operador feche o jogo. |
| Processo de teste ficou travado após pedido de parada | Permitir encerramento forçado em execução automatizada com essa opção habilitada. Restringir ao processo iniciado pelo próprio executor de testes, conferindo PID e horário de criação antes de agir. |

## Etapas de implementação

### 1. Padronizar o resultado da sessão

- [ ] Reaproveitar o orquestrador em [lab-load.ps1](../../../internal/lab-load.ps1). Fazer a pré-checagem e a carga produzirem eventos estruturados com ID da tentativa, horário, etapa, resultado, código, PID e caminho do log. Reusar essa saída na linha de comando e na janela.
- [ ] Usar os estados `verificando`, `aguardando_jogo`, `carregando`, `aguardando_painel`, `pronto`, `falhou`, `indeterminado` e `encerrado`. Exigir o evento `first_frame` da tentativa atual para chegar a `pronto`. Tratar registro incompleto como pendente dentro do prazo.
- [ ] Separar tempo limite de erro definitivo no [carregador auxiliar](../../../internal/lab-loader.cpp). Serializar a carga por instância do CS2 com PID e horário de criação. Proteger tanto duplo clique quanto tentativas vindas de janelas diferentes.
- [ ] Validar com eventos simulados. Um log antigo nunca produz `pronto`, dois pedidos simultâneos disparam no máximo uma carga e tempo limite não dispara repetição automática.

### 2. Verificar o pacote e exibir o estado

- [ ] Usar os hashes de DLL, carregador e script do manifesto gerado por [build-release.ps1](../../../tools/build-release.ps1). Conferir a versão do pacote e comparar a build instalada do CS2 com a build testada. Registrar a build testada como dado próprio se ela divergir da build de origem do catálogo.
- [ ] Preservar a execução pelo checkout de desenvolvimento com identificação explícita. Exigir manifesto válido na edição empacotada.
- [ ] Trocar a abertura do console por uma janela no [iniciador nativo](../../../internal/acervo-launcher.cpp). Mostrar versão, estado, etapa atual e ações **Verificar**, **Carregar** e **Copiar diagnóstico**. Executar a verificação fora da interface para que a janela continue respondendo.
- [ ] Testar o ZIP recém-extraído, um pacote com arquivo alterado, uma build desconhecida e a abertura com o CS2 fechado. A ação **Verificar** deve terminar sem carregar a DLL.

### 3. Tornar a parada verificável

- [ ] Revisar a limpeza parcial em [entry.cpp](../../../internal/entry.cpp). Desmontar apenas componentes que inicializaram e emitir a confirmação de parada após a limpeza. O evento `session_closed` atual precede a chamada de limpeza.
- [ ] Manter a supervisão fora da DLL. Diferenciar pedido de parada, desmontagem confirmada, saída do processo e fim do prazo. Guardar a causa e a última etapa observada por tentativa.
- [ ] Testar encerramento e travamento com um processo auxiliar descartável antes do CS2. Forçar timeout de parada e conferir que o executor só termina o processo que criou. Se a desmontagem falhar em uma sessão aberta manualmente, exibir a instrução de reinício.

### 4. Validar e liberar a versão de laboratório

- [ ] Repetir em mapa local com a build registrada. Cobrir carga normal, jogo fechado durante a inicialização, primeiro quadro ausente, tentativa duplicada, parada e nova sessão após reiniciar.
- [ ] Registrar para cada cenário versão do Hy3x, build do jogo, estado final e log. Conferir visualmente o painel e a aplicação de um perfil exportado.
- [ ] Gerar o ZIP pelo empacotador existente e repetir **Verificar** e a carga em uma pasta recém-extraída. Guardar o manifesto, o pacote e os resultados juntos.

## Ponto de retomada

O pacote `0.1.2-lab` foi compilado e extraído em `dist/Hy3x-0.1.2-lab/Windows-x64`. O manifesto conferiu os 526 arquivos. O [script de sessão](../../../internal/lab-load.ps1) chama a [pré-checagem](../../../internal/lab-preflight.ps1) antes do carregador. Ela lê o BuildID no manifesto Steam da instalação e confere o SHA-256 da DLL nos pacotes extraídos. A build instalada `26598758` e a DLL do pacote passaram nas verificações. O teste simulado bloqueou build incompatível e DLL alterada.

Marco confirmou que as skins mudaram e **End** fechou o painel com o jogo aberto na sessão cujo log `sessao-30372.log` registra `0.1.0-lab`. Em seguida, confirmou o teste de aplicação no pacote novo. O log `sessao-12592.log` registra `initializing 0.1.2-lab`, `catalog_ready` e `first_frame`. Marco informou que não acionou **End** nessa sessão. A desmontagem na `0.1.2-lab` ainda precisa de teste.

O pacote `0.1.3-lab` usa eventos estruturados do script de sessão na [janela](../../../internal/lab-window.ps1). Cada tentativa traz ID, horário, etapa e resultado, com PID, caminho do log e código de saída quando disponíveis. Os testes passaram na pré-checagem real e nos cenários simulados de espera expirada e repetição bloqueada. Os 526 arquivos extraídos conferiram. Marco confirmou o teste local de aplicação de skin e encerramento pela tecla **End**. O log `sessao-35036.log` registra a versão `0.1.3-lab`, o primeiro quadro, o pedido de encerramento e a limpeza concluída 654 ms depois.

O pacote `0.1.4-observe` foi compilado, extraído em `dist/Hy3x-0.1.4-observe/Windows-x64` e verificado pelo manifesto. Na variante habilitada por `-ObserveItems`, o [hook de quadro](../../../internal/hooks/hooks.cpp) chama o [observador de atributos](../../../internal/features/shared/econ_item_attribute_manager.cpp) no lugar dos aplicadores. O log registra apenas tipo, definição, contagem e presença de dados, sem despejo de bytes ou endereço. A compilação normal também passou e o binário normal não contém os marcadores de observação. Falta executar a coleta em mapa local e estabelecer a propriedade e a capacidade do vetor com evidência independente.

As edições `0.1.5-lab` e `0.1.5-observe` corrigem o arrasto do menu. O [menu](../../../internal/menu/menu.cpp) agora centraliza a janela só na primeira abertura sem posição salva. Uma simulação com o ImGui do projeto reproduziu o bloqueio anterior e confirmou arrasto pelo título, posição preservada ao reabrir e clique nas abas. Os dois pacotes foram extraídos e verificados. Usar `0.1.5-observe` para a coleta pendente.

Marco confirmou o teste da edição normal `0.1.5-lab`. O log `sessao-4156.log` registra duas inicializações da `0.1.5-observe`, uma desmontagem concluída e observações de armas com definições `4`, `7`, `49` e `59`, todas com `count=0 data_present=0`. A amostra de luvas com definição `0` também estava vazia. A coleta funcionou e ainda falta uma amostra com atributos preexistentes. A interface `0.1.6-observe` informa que a aplicação está desativada no lugar dos controles que enfileiravam skins ou kits sem execução. A ideia de inventário em lote foi adiada por Marco até depois da etapa online.
