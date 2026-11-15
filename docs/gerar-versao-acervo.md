# Gerar uma versão local do Acervo

Na raiz, em PowerShell:

```powershell
.\tools\build-release.ps1 -Version 0.1.0-lab
```

O script usa Visual Studio 2022, MSVC x64 `14.44.35207`, Windows SDK `10.0.26100.0`, Git, Node 24 e Python 3. Copia os insumos para uma pasta nova, roda os testes, compila e verifica a extração de cada ZIP pelos hashes do manifesto. O destino `dist/Hy3x-0.1.0-lab` é publicado ao fim da validação. Use outro número para a próxima versão.

- `*-windows-x64.zip` contém o iniciador, o carregador, o módulo, os recursos, a bancada web e o guia. O painel nativo exige o Microsoft Visual C++ Redistributable x64.
- `*-source.zip` contém o código, testes, ativos de entrada e ferramentas para repetir o build.
- `*-source.bundle` é um snapshot Git com commit e tag da versão. Para abrir a fonte com histórico local, execute `git clone Hy3x-0.1.0-lab-source.bundle Hy3x-fonte`.
- `manifest.json` registra versão, origem, catálogo, ambiente de build e SHA-256 por arquivo. O hash da árvore identifica o conteúdo-fonte exato.

O script versiona o snapshot em um repositório separado. O histórico e as alterações em andamento em `internal/.git` ficam disponíveis na árvore de trabalho original.

## Verificar um pacote extraído

Na raiz do projeto, informe a pasta que contém o `manifest.json` e o `Acervo-Laboratorio.exe`.

```powershell
.\tools\build-release.ps1 -VerifyDirectory 'C:\caminho\Hy3x-extraido'
```

O comando mostra a versão e confere os arquivos da pasta com o manifesto. Se houver diferenças, ele indica os caminhos afetados. A verificação roda com o CS2 fechado.

## Catálogo

O snapshot nativo está em `internal/menu/acervo_finishes.inc`. A seleção curada da bancada fica em `internal/tools/web-collection.json`, com IDs de arquivos preservados para abrir presets antigos. O catálogo gerado usa do snapshot os nomes, grupos e limites do acabamento.

```powershell
python internal/tools/catalog_outputs.py --check
```

Para regenerar a partir de outra build, instale o ambiente de catálogo e forneça um diretório de saída novo. O argumento `--build` deve corresponder à build do VPK de entrada.

```powershell
python -m pip install -r internal/tools/requirements-catalog.txt
python internal/tools/prepare_collection.py 'D:/SteamLibrary/steamapps/common/Counter-Strike Global Offensive/game/csgo/pak01_dir.vpk' --build 26598758 --output dist/catalog-26598758-novo
```

Depois de revisar o snapshot publicado, copie juntos o índice, a pasta de miniaturas e o manifesto para os caminhos correspondentes em `internal/`, e `web-catalog.js` para `ui/catalog.js`. O comando `--check` verifica o resultado antes de gerar uma versão.
