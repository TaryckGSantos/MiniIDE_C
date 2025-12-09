#ifndef SEMANTICO_H
#define SEMANTICO_H
#include "Token.h"
#include "SemanticError.h"

class CodeGeneratorBIP;

#include <vector>
#include <string>
#include <ostream>
#include <algorithm>
#include <functional>
#include <map>

class Simbolo {
public:
    std::string tipo;
    std::string nome;
    bool usado = false;
    bool inicializado = false;
    std::string modalidade;   // "variavel", "parametro", "funcao", "vetor", etc.
    bool isVetor = false;
    int  vetorTam = 0;
    std::string escopo;       // "global" ou nome_da_funcao

    friend std::ostream& operator<<(std::ostream& os, const Simbolo& s);
};

enum class TipoBase {
    T_INT,
    T_FLOAT,
    T_CHAR,
    T_BOOL,
    T_STRING,
    T_DOUBLE,
    T_LONG,
    T_VOID,
    T_DESCONHECIDO
};

struct FuncSignature {
    std::string returnType;              // tipo de retorno: "int", "void", etc.
    std::vector<std::string> paramTypes; // tipos dos parâmetros, na ordem
};

class Semantico {
private:
    // Helpers de busca/escopo
    bool existeNoEscopoAtual(const std::string& nome) const;
    bool existe(const std::string& nome) const;
    std::string escopoAtual() const;

    // busca de símbolo com retorno do símbolo encontrado
    bool buscarSimbolo(const std::string& nome, Simbolo& out) const;

    // impede sombreamento dentro da MESMA FUNÇÃO
    bool existeNoEscopoDaFuncaoAtual(const std::string& nome) const;

    // ===== Estado do analisador =====
    bool        modoDeclaracao = false;
    std::string tipoAtual;
    int         lastDeclaredPos = -1;
    std::string ultimoDeclaradoNome;

    // pilhas de escopos/blocos e funções
    std::vector<std::vector<Simbolo>> pilhaEscopos;
    std::vector<std::string>          pilhaFuncoes;
    std::vector<bool>                 pilhaEscopoEhFuncao;

    // tabela linear opcional (histórico/relatório)
    std::vector<Simbolo>              tabelaLinear;

    // controle de listas de inicialização
    bool inInitList      = false;
    int  initListDepth   = 0;
    bool pendingInitList = false;

    // logging/mensagens
    void info(const std::string& msg) const;
    void warn(const std::string& msg) const;
    void error(const std::string& msg) const;
    void addMsg(const std::string& msg) const { mensagens_.push_back(msg); }

    // declar/acabamento de declaração
    void endDeclaracao();
    void beginDeclaracao(const std::string& tipo);

    // usado no case 10/colchetes: promove último declarado a "vetor"
    void marcarUltimoDeclaradoComoVetor(const std::string& nome);

    CodeGeneratorBIP* codeGen = nullptr;

    // mapa de assinaturas de função
    std::map<std::string, FuncSignature> funcoes_;

    // estado para chamada de função
    std::string funcEmChamada_;
    bool        inCallArgs_    = false;
    int         callArgsCount_ = 0;

    // inferência de tipo da expressão atual (para argumentos de função)
    TipoBase currentExprType_ = TipoBase::T_DESCONHECIDO;
    std::vector<TipoBase> callArgTypes_;

public:
    // tabela “global” que você já usa
    std::vector<Simbolo> tabelaSimbolo;

    // API principal
    void executeAction(int action, const Token* token);
    void abrirEscopo() { pilhaEscopos.push_back({}); }
    void fecharEscopo();
    void verificarNaoUsados() const;

    void setCodeGenerator(CodeGeneratorBIP* cg) { codeGen = cg; }

    // operações principais
    void declarar(const Token* tok);
    void usar(const Token* tok);

    // logging/mensagens
    void setLogger(std::function<void(const std::string&)> fn) { logger_ = std::move(fn); }
    const std::vector<std::string>& mensagens() const { return mensagens_; }

    bool temErro() const { return temErro_; }

    void clearMensagens() {
        mensagens_.clear();
        temErro_ = false;   // zera flag sempre que limpar mensagens
    }

private:
    mutable std::function<void(const std::string&)> logger_;
    mutable std::vector<std::string> mensagens_;
    mutable bool temErro_ = false;
};

#endif
