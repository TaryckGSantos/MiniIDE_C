# MiniIDE_C

## Introdução

* Trabalho feito até a geração de código 3

MiniIDE_C é uma mini IDE desenvolvida em C++ que permite a edição e compilação de códigos em C++. Ao compilar o código, o projeto gera:

- Uma **tabela de símbolos** contendo:  
  nome do símbolo, tipo (int, float, void, etc.), modalidade (função, parâmetro, variável, etc.), escopo (global ou dentro de uma função específica), se o símbolo é utilizado em algum momento e se ele é inicializado.
- Um **assembly** baseado no assembly do BIP.

Este projeto foi desenvolvido e testado exclusivamente no Windows 10 e 11 (64-bit).

## Requisitos do Sistema

- Windows 10 ou 11 (64-bit)
- 8 GB de RAM recomendado
- ~10 GB de espaço livre em disco (para Qt + ferramentas)

## Tecnologias Utilizadas

- Qt 6.9.2 MinGW 64-bit (Debug)
- Qt Creator 17.0.1
- CMake (para build)
- Analisador léxico/sintático/semântico gerado pelo WEB GALS
- MinGW 64-bit (incluído no instalador do Qt)

## Estrutura do Projeto
MiniIDE_C/
- ├── CMakeLists.txt
- ├── MiniIDE.pro
- ├── MiniIDE_pt_BR.ts
- └── MiniIDE/
- ├── Forms/
- │   └── mainwindow.ui
- ├── Headers/
- │   ├── AnalysisError.h
- │   ├── codegeneratorbip.h
- │   ├── Constants.h
- │   ├── LexicalError.h
- │   ├── Lexer.h
- │   ├── SemanticError.h
- │   ├── Semantico.h        
- │   ├── SyntacticError.h
- │   ├── Token.h
- │   └── mainwindow.h
- └── Source/
- ├── codegeneratorbip.cpp
- ├── Constants.cpp
- ├── Lexer.cpp
- ├── Semantico.cpp        
- ├── Sintatico.cpp
- ├── main.cpp
- └── mainwindow.cpp


## Instalação Completa (Passo a Passo)

### 1. Instalar o Git
```bash
# Baixe em: https://git-scm.com/download/win
# Execute o instalador usando as opções padrão
# Teste no Prompt de Comando:
git --versionb
```
### 2. Baixar e Instalar Qt 6.9.2 + Qt Creator 17.0.1

- Acesse https://www.qt.io/download
- Baixe o instalador open-source (qt-unified-windows-x64-online.exe)
- Faça login ou crie uma conta gratuita
- Na tela de componentes, selecione:
- Qt → Qt 6.9.2 → MinGW 64-bit
- Developer and Designer Tools → Qt Creator 17.0.1

Instale (pode demorar 30–60 minutos)

Caminho padrão de instalação: C:\Qt

### 3. Clonar o Repositório
# Escolha onde quer salvar o projeto

Abra o terminal do Git Bash e cole:

cd C:\Users\SEU_USUARIO\Projetos

git clone https://github.com/TaryckGSantos/MiniIDE_C.git

cd MiniIDE_C

### 4. Abrir e Configurar no Qt Creator

- Abra o Qt Creator 17.0.1
- File → Open File or Project…
- Selecione o arquivo CMakeLists.txt da pasta clonada
- Na janela de configuração do kit:
- Marque o kit Desktop Qt 6.9.2 MinGW 64-bit (Debug)
- Caso não apareça, vá em Tools → Options → Kits e adicione manualmente

- Clique em “Configure Project”

### 5. Compilar e Executar

- Verifique se o modo está em Debug (canto inferior esquerdo)
- Clique no martelo verde (Build) ou pressione Ctrl + B
- Após o build terminar sem erros, clique na seta verde (Run) ou pressione Ctrl + R

Pronto! A MiniIDE_C será aberta e você já pode usar.
