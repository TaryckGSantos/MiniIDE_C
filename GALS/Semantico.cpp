#include "Semantico.h"
#include "Token.h"
#include "SemanticError.h"

#include "CodeGeneratorBIP.h"

#include <iostream>
#include <algorithm>
#include <string>

// ==== helpers de tipos ====
static TipoBase stringToTipoBase(const std::string& t) {
    std::string s = t;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    if (s == "int")    return TipoBase::T_INT;
    if (s == "float")  return TipoBase::T_FLOAT;
    if (s == "char")   return TipoBase::T_CHAR;
    if (s == "bool")   return TipoBase::T_BOOL;
    if (s == "string") return TipoBase::T_STRING;
    if (s == "double") return TipoBase::T_DOUBLE;
    if (s == "long")   return TipoBase::T_LONG;
    if (s == "void")   return TipoBase::T_VOID;
    return TipoBase::T_DESCONHECIDO;
}

static std::string tipoBaseToString(TipoBase t) {
    switch (t) {
    case TipoBase::T_INT:    return "int";
    case TipoBase::T_FLOAT:  return "float";
    case TipoBase::T_CHAR:   return "char";
    case TipoBase::T_BOOL:   return "bool";
    case TipoBase::T_STRING: return "string";
    case TipoBase::T_DOUBLE: return "double";
    case TipoBase::T_LONG:   return "long";
    case TipoBase::T_VOID:   return "void";
    default:                 return "desconhecido";
    }
}

// "promoção" simples de tipos: se um for desconhecido, pega o outro;
// se forem iguais, mantém; se forem numéricos diferentes, assume o "maior"
static bool isNumeric(TipoBase t) {
    return t == TipoBase::T_INT   ||
           t == TipoBase::T_FLOAT ||
           t == TipoBase::T_DOUBLE ||
           t == TipoBase::T_LONG;
}

static int numericRank(TipoBase t) {
    switch (t) {
    case TipoBase::T_INT:    return 1;
    case TipoBase::T_LONG:   return 2;
    case TipoBase::T_FLOAT:  return 3;
    case TipoBase::T_DOUBLE: return 4;
    default:                 return 0;
    }
}

static TipoBase promoverTipos(TipoBase a, TipoBase b) {
    if (a == TipoBase::T_DESCONHECIDO) return b;
    if (b == TipoBase::T_DESCONHECIDO) return a;
    if (a == b) return a;

    if (isNumeric(a) && isNumeric(b)) {
        return (numericRank(a) >= numericRank(b)) ? a : b;
    }
    // tipos não numéricos/diferentes -> por simplicidade, marca desconhecido
    return TipoBase::T_DESCONHECIDO;
}

// compatibilidade "estrita": mesmo tipo, ou (opcional) promoções numéricas
static bool tiposCompativeis(TipoBase esperado, TipoBase recebido) {
    // se não conseguimos inferir o tipo da expressão, não trava o compilador
    if (recebido == TipoBase::T_DESCONHECIDO) {
        return true;
    }

    //  só aceita se for exatamente o mesmo tipo
    return (esperado == recebido);
}


static std::string g_ultimoIdVisto;
static std::string g_ultimoIdAntesDaAtrib;
static bool        g_inParamList = false;
static bool        g_nextBraceIsFuncBody = false;
static std::string g_funcEmConstrucao;
static std::vector<Simbolo> g_paramBuffer;

std::ostream& operator<<(std::ostream& os, const Simbolo& s) {
    os << "Tipo: " << s.tipo
       << " - Nome: " << s.nome
       << " - Modalidade: " << s.modalidade
       << " - Escopo: " << s.escopo
       << " - Usado: " << (s.usado ? "Sim" : "Não")
       << " - Inicializado: " << (s.inicializado ? "Sim" : "Não");
    return os;
}

// Promove o último ID declarado para FUNÇÃO
static void promoverParaFuncao(
    const std::string& nomeFunc,
    std::vector<std::vector<Simbolo>>& pilhaEscopos,
    std::vector<Simbolo>& tabelaSimbolo
    ){
    if (nomeFunc.empty() || pilhaEscopos.empty()) return;
    for (auto& s : pilhaEscopos.back()) {
        if (s.nome == nomeFunc) {
            s.modalidade = "funcao";
            s.escopo = "global";
            s.inicializado = true;
            for (auto& t : tabelaSimbolo)
                if (t.nome == s.nome && t.tipo == s.tipo && t.escopo == "global")
                { t.modalidade = "funcao"; t.inicializado = true; }
            return;
        }
    }
}

// percorrer da pilha mais interna para a mais externa
static void marcarUsadoPorNome(
    const std::string& nome,
    std::vector<std::vector<Simbolo>>& pilhaEscopos,
    std::vector<Simbolo>& tabelaSimbolo
    ) {
    if (nome.empty()) return;
    for (auto it = pilhaEscopos.rbegin(); it != pilhaEscopos.rend(); ++it) {
        for (auto& simbolo : *it) {
            if (simbolo.nome == nome) {
                simbolo.usado = true;
                for (auto& s : tabelaSimbolo)
                    if (s.nome == simbolo.nome && s.tipo == simbolo.tipo && s.escopo == simbolo.escopo)
                        s.usado = true;
                return;
            }
        }
    }
}

static void marcarInicializadoPorNome(
    const std::string& nome,
    std::vector<std::vector<Simbolo>>& pilhaEscopos,
    std::vector<Simbolo>& tabelaSimbolo
    ) {
    if (nome.empty()) return;
    for (auto it = pilhaEscopos.rbegin(); it != pilhaEscopos.rend(); ++it) {
        for (auto& simbolo : *it) {
            if (simbolo.nome == nome) {
                simbolo.inicializado = true;
                std::cerr << "Marcando " << nome << " como inicializado no escopo " << simbolo.escopo << std::endl;
                for (auto& s : tabelaSimbolo)
                    if (s.nome == simbolo.nome && s.tipo == simbolo.tipo && s.escopo == simbolo.escopo)
                        s.inicializado = true;
                return;
            }
        }
    }
}

bool Semantico::buscarSimbolo(const std::string& nome, Simbolo& out) const {
    if (nome.empty()) return false;
    for (auto it = pilhaEscopos.rbegin(); it != pilhaEscopos.rend(); ++it) {
        for (const auto& s : *it) {
            if (s.nome == nome) {
                out = s;
                return true;
            }
        }
    }
    return false;
}


// Nova função para marcar inicialização de elementos de vetor
static void marcarElementoVetorInicializado(
    const std::string& nome,
    int /*indice*/,
    std::vector<std::vector<Simbolo>>& pilhaEscopos,
    std::vector<Simbolo>& tabelaSimbolo
    ) {
    if (nome.empty()) return;
    for (auto it = pilhaEscopos.rbegin(); it != pilhaEscopos.rend(); ++it) {
        for (auto& simbolo : *it) {
            if (simbolo.nome == nome && simbolo.modalidade == "vetor") {
                simbolo.inicializado = true;
                std::cerr << "Marcando elemento de " << nome << " como inicializado no escopo " << simbolo.escopo << std::endl;
                for (auto& s : tabelaSimbolo)
                    if (s.nome == simbolo.nome && s.tipo == simbolo.tipo && s.escopo == simbolo.escopo)
                        s.inicializado = true;
                return;
            }
        }
    }
}

bool Semantico::existeNoEscopoAtual(const std::string& nome) const {
    if (pilhaEscopos.empty()) return false;
    const auto& esc = pilhaEscopos.back();
    return std::any_of(esc.begin(), esc.end(), [&](const Simbolo& s){ return s.nome == nome; });
}
bool Semantico::existe(const std::string& nome) const {
    for (auto it = pilhaEscopos.rbegin(); it != pilhaEscopos.rend(); ++it) {
        const auto& esc = *it;
        if (std::any_of(esc.begin(), esc.end(), [&](const Simbolo& s){ return s.nome == nome; }))
            return true;
    }
    return false;
}

// impede sombreamento na MESMA FUNÇÃO
bool Semantico::existeNoEscopoDaFuncaoAtual(const std::string& nome) const {
    const std::string esc = escopoAtual();
    if (esc == "global") return false;
    for (auto it = pilhaEscopos.rbegin(); it != pilhaEscopos.rend(); ++it) {
        for (const auto& s : *it) {
            if (s.nome == nome && s.escopo == esc) {
                return true;
            }
        }
    }
    return false;
}

void Semantico::marcarUltimoDeclaradoComoVetor(const std::string& nome) {
    if (nome.empty() || pilhaEscopos.empty()) return;
    auto& escopoAtualRef = pilhaEscopos.back();
    for (auto& sim : escopoAtualRef) {
        if (sim.nome == nome) {
            sim.modalidade = "vetor";
            for (auto& s : tabelaSimbolo)
                if (s.nome == sim.nome && s.tipo == sim.tipo && s.escopo == sim.escopo)
                    s.modalidade = "vetor";
            return;
        }
    }
}

// Escopo atual: "global" ou nome da função do topo da pilha
std::string Semantico::escopoAtual() const {
    if (!pilhaFuncoes.empty()) return pilhaFuncoes.back();
    return "global";
}

void Semantico::beginDeclaracao(const std::string& tipo) {
    modoDeclaracao   = true;
    tipoAtual        = tipo;
    lastDeclaredPos  = -1;
    ultimoDeclaradoNome.clear();
    // ao iniciar uma declaração, zera flags de lista de init
    inInitList = false;
    pendingInitList = false;
    initListDepth = 0;
}

void Semantico::endDeclaracao() {
    modoDeclaracao   = false;
    tipoAtual.clear();
    lastDeclaredPos  = -1;
    ultimoDeclaradoNome.clear();
    // garante estado consistente
    inInitList = false;
    pendingInitList = false;
    initListDepth = 0;
}

// Semantico: declarar/usar/fechar
void Semantico::declarar(const Token* tok) {
    if (!tok) return;
    if (tok->getId() != t_ID) return;

    const std::string nome = tok->getLexeme();
    if (nome.empty()) return;

    if (pilhaEscopos.empty()) abrirEscopo();

    // (1) Duplicidade no BLOCO atual
    if (existeNoEscopoAtual(nome)) {
        throw SemanticError("Símbolo '" + nome + "' já existe neste escopo",
                            tok->getPosition());
    }

    // (2) Proibir sombreamento dentro da MESMA FUNÇÃO
    if (escopoAtual() != "global" && existeNoEscopoDaFuncaoAtual(nome)) {
        throw SemanticError(
            "Símbolo '" + nome + "' já foi declarado anteriormente na função '" + escopoAtual() + "'.",
            tok->getPosition()
            );
    }

    if (tipoAtual.empty()) {
        throw SemanticError("Declaração de '" + nome + "' sem tipo corrente",
                            tok->getPosition());
    }

    Simbolo sim;
    sim.tipo = tipoAtual;
    sim.nome = nome;
    sim.usado = false;
    sim.inicializado = false;
    sim.modalidade = "variavel";
    sim.escopo = escopoAtual();

    pilhaEscopos.back().push_back(sim);
    tabelaSimbolo.push_back(sim);

    g_ultimoIdVisto = nome;
    g_ultimoIdAntesDaAtrib = nome;
    ultimoDeclaradoNome = nome;
}

void Semantico::usar(const Token* tok) {
    const std::string nome = tok->getLexeme();
    if (nome.empty()) return;

    bool encontrado = false;
    for (auto it = pilhaEscopos.rbegin(); it != pilhaEscopos.rend(); ++it) {
        for (auto& simbolo : *it) {
            if (simbolo.nome == nome) {
                if (!simbolo.inicializado) {
                    warn("Aviso: Símbolo '" + nome +
                         "' (tipo: " + simbolo.tipo +
                         ", escopo: " + simbolo.escopo +
                         ") usado sem inicialização na posição " +
                         std::to_string(tok->getPosition()));
                }
                simbolo.usado = true;
                for (auto& s : tabelaSimbolo)
                    if (s.nome == simbolo.nome && s.tipo == simbolo.tipo && s.escopo == simbolo.escopo)
                        s.usado = true;
                encontrado = true;
                break;
            }
        }
        if (encontrado) break;
    }
    if (!encontrado) {
        throw SemanticError("'" + nome + "' não declarado neste escopo", tok->getPosition());
    }
}

void Semantico::fecharEscopo() {
    if (pilhaEscopos.empty()) return;

    for (const auto& simbolo : pilhaEscopos.back()) {
        if (!simbolo.usado) {
            warn("Aviso: Símbolo '" + simbolo.nome +
                 "' (tipo: " + simbolo.tipo +
                 ", escopo: " + simbolo.escopo +
                 ") declarado mas não usado.");
        }
    }
    pilhaEscopos.pop_back();

    if (!pilhaEscopoEhFuncao.empty()) {
        bool eraFunc = pilhaEscopoEhFuncao.back();
        pilhaEscopoEhFuncao.pop_back();
        if (eraFunc && !pilhaFuncoes.empty())
            pilhaFuncoes.pop_back();
    }
}

void Semantico::verificarNaoUsados() const {
    for (const auto& simbolo : tabelaSimbolo) {
        if (!simbolo.usado) {
            warn("Aviso: Símbolo '" + simbolo.nome +
                 "' (tipo: " + simbolo.tipo +
                 ", escopo: " + simbolo.escopo +
                 ") declarado mas não usado.");
        }
    }
}

void Semantico::warn(const std::string& msg) const {
    std::cerr << "[WARN] " << msg << std::endl;
    mensagens_.push_back("Aviso: " + msg);
    if (logger_) logger_(std::string("Aviso: ") + msg);
}

void Semantico::error(const std::string& msg) const {
    std::cerr << "[ERRO] " << msg << std::endl;
    mensagens_.push_back("Erro: " + msg);
    temErro_ = true;
    if (logger_) logger_(std::string("Erro: ") + msg);
}

void Semantico::executeAction(int action, const Token* token)
{
    if (token) {
        const int id = token->getId();
        switch (id) {
        // TIPOS
        case t_KEY_INT:
        case t_KEY_FLOAT:
        case t_KEY_CHAR:
        case t_KEY_STRING:
        case t_KEY_BOOL:
        case t_KEY_DOUBLE:
        case t_KEY_LONG:
        case t_KEY_VOID:
            beginDeclaracao(token->getLexeme());
            break;
        // PARENTS (assinatura)
        case t_DELIM_PARENTESESE:
            if (modoDeclaracao) {
                g_inParamList = true;
                g_paramBuffer.clear();
                g_funcEmConstrucao = g_ultimoIdVisto;
                promoverParaFuncao(g_funcEmConstrucao, pilhaEscopos, tabelaSimbolo);
            }
            break;
        case t_DELIM_PARENTESESD:
            if (g_inParamList) {
                // fim da lista de parâmetros da DECLARAÇÃO de função
                g_inParamList = false;
                g_nextBraceIsFuncBody = true;
                // REGISTRA ASSINATURA DA FUNÇÃO
                if (!g_funcEmConstrucao.empty()) {
                    // 1) Descobrir tipo de retorno da função
                    std::string retType;
                    for (auto it = pilhaEscopos.rbegin(); it != pilhaEscopos.rend(); ++it) {
                        for (const auto& s : *it) {
                            if (s.nome == g_funcEmConstrucao && s.modalidade == "funcao" && s.escopo == "global") {
                                retType = s.tipo;
                                break;
                            }
                        }
                        if (!retType.empty()) break;
                    }
                    if (retType.empty()) {
                        // fallback se por algum motivo não achar
                        retType = "int";
                    }
                    FuncSignature sig;
                    sig.returnType = retType;
                    // 2) Tipos dos parâmetros (em ordem)
                    sig.paramTypes.clear();
                    // Preferência: usar g_paramBuffer se ele tiver algo
                    if (!g_paramBuffer.empty()) {
                        for (const auto& p : g_paramBuffer) {
                            sig.paramTypes.push_back(p.tipo);
                        }
                    } else {
                        // fallback robusto: pega da tabela de símbolos
                        // todos os símbolos que são parâmetros da função
                        for (const auto& s : tabelaSimbolo) {
                            if (s.modalidade == "parametro" &&
                                s.escopo == g_funcEmConstrucao)
                            {
                                sig.paramTypes.push_back(s.tipo);
                            }
                        }
                    }
                    // 3) Salva no mapa (detecção de redeclaração opcional)
                    auto it = funcoes_.find(g_funcEmConstrucao);
                    if (it != funcoes_.end()) {
                        error("Função '" + g_funcEmConstrucao + "' já foi declarada anteriormente.");
                    } else {
                        funcoes_[g_funcEmConstrucao] = sig;
                    }
                }
            }
            // fim de qualquer declaração que esteja em andamento
            endDeclaracao();
            break;
        // IDENTIFICADORES
        case t_ID:
            if (g_inParamList) {
                if (tipoAtual.empty())
                    throw SemanticError("Parâmetro sem tipo declarado", token->getPosition());
                if (lastDeclaredPos != token->getPosition()) {
                    Simbolo p;
                    p.tipo = tipoAtual; p.nome = token->getLexeme();
                    p.usado = false; p.inicializado = true;
                    p.modalidade = "parametro";
                    p.escopo = g_funcEmConstrucao.empty() ? "global" : g_funcEmConstrucao;
                    g_paramBuffer.push_back(p);
                    tabelaSimbolo.push_back(p);
                    lastDeclaredPos = token->getPosition();
                }
            } else if (modoDeclaracao && lastDeclaredPos != token->getPosition()) {
                declarar(token);
                lastDeclaredPos = token->getPosition();
                g_ultimoIdVisto = token->getLexeme();
                g_ultimoIdAntesDaAtrib = g_ultimoIdVisto;
                ultimoDeclaradoNome = g_ultimoIdVisto;
            } else {
                usar(token);
                g_ultimoIdVisto = token->getLexeme();
                g_ultimoIdAntesDaAtrib = g_ultimoIdVisto; // Atualiza antes da atribuição
                // Se estamos em argumentos de chamada, usa o tipo do ID na expressão atual
                if (inCallArgs_) {
                    Simbolo sim;
                    if (buscarSimbolo(token->getLexeme(), sim)) {
                        TipoBase t = stringToTipoBase(sim.tipo);
                        currentExprType_ = promoverTipos(currentExprType_, t);
                    }
                }
            }
            break;
        // VÍRGULA
        case t_DELIM_VIRGULA:
            if (modoDeclaracao || g_inParamList) {
                lastDeclaredPos = -1;
                ultimoDeclaradoNome.clear();
            }
            break;
        // PONTO E VÍRGULA
        case t_DELIM_PONTOVIRGULA:
            endDeclaracao();
            g_ultimoIdVisto.clear();
            g_ultimoIdAntesDaAtrib.clear();
            break;
        // CHAVES
        case t_DELIM_CHAVEE: {
            if (modoDeclaracao && (pendingInitList || inInitList)) {
                inInitList = true;
                ++initListDepth;
                break;
            }
            abrirEscopo();
            bool ehFunc = false;
            if (g_nextBraceIsFuncBody) {
                ehFunc = true;
                g_nextBraceIsFuncBody = false;
                if (!g_funcEmConstrucao.empty())
                    pilhaFuncoes.push_back(g_funcEmConstrucao);
                auto& escopoAtual = pilhaEscopos.back();
                for (const auto& p : g_paramBuffer) {
                    bool dup = std::any_of(escopoAtual.begin(), escopoAtual.end(),
                                           [&](const Simbolo& s){ return s.nome == p.nome; });
                    if (!dup) escopoAtual.push_back(p);
                }
                g_paramBuffer.clear();
                ultimoDeclaradoNome.clear();
            }
            pilhaEscopoEhFuncao.push_back(ehFunc);
            break;
        }
        case t_DELIM_CHAVED:
            if (inInitList) {
                if (initListDepth > 0) --initListDepth;
                if (initListDepth == 0) {
                    inInitList = false; pendingInitList = false;
                    if (!ultimoDeclaradoNome.empty())
                        marcarInicializadoPorNome(ultimoDeclaradoNome, pilhaEscopos, tabelaSimbolo);
                }
                break;
            }
            fecharEscopo();
            g_ultimoIdVisto.clear();
            g_ultimoIdAntesDaAtrib.clear();
            break;
        // '='
        case t_OPR_ATRIB:
            if (modoDeclaracao) {
                pendingInitList = true;
                if (!ultimoDeclaradoNome.empty()) {
                    marcarInicializadoPorNome(ultimoDeclaradoNome, pilhaEscopos, tabelaSimbolo);
                } else if (!g_ultimoIdVisto.empty()) {
                    marcarInicializadoPorNome(g_ultimoIdVisto, pilhaEscopos, tabelaSimbolo);
                }
            }
            break;
        // '['
        case t_DELIM_COLCHETESE:
            if (modoDeclaracao) {
                const std::string alvo = !ultimoDeclaradoNome.empty() ? ultimoDeclaradoNome : g_ultimoIdVisto;
                marcarUltimoDeclaradoComoVetor(alvo);
            } else {
                marcarUsadoPorNome(g_ultimoIdVisto, pilhaEscopos, tabelaSimbolo);
            }
            break;
        // (apenas reconhece os tokens; código de desvio está no MainWindow/emitirTextBasico)
        case t_OPR_MAIOR: // '>'
        case t_OPR_MENOR: // '<'
        case t_OPR_MAIOR_IGUAL: // '>='
        case t_OPR_MENOR_IGUAL: // '<='
        case t_OPR_IGUAL: // '=='
        case t_OPR_DIFERENTE: // '!='
            break;
        // LITERAIS / CONSTANTES
        case t_LIT_INTEIRO:
        case t_HEXADECIMAL:
        case t_BINARIO:
            if (inCallArgs_) {
                currentExprType_ = promoverTipos(currentExprType_, TipoBase::T_INT);
                std::cerr << "[DEBUG] Literal inteiro em argumento, tipo atual = "
                          << tipoBaseToString(currentExprType_) << "\n";
            }
            break;
        case t_LIT_DECIMAIS:
            if (inCallArgs_) {
                currentExprType_ = promoverTipos(currentExprType_, TipoBase::T_FLOAT);
                std::cerr << "[DEBUG] Literal decimal em argumento, tipo atual = "
                          << tipoBaseToString(currentExprType_) << "\n";
            }
            break;
        case t_CHAR:
            if (inCallArgs_) {
                currentExprType_ = promoverTipos(currentExprType_, TipoBase::T_CHAR);
            }
            break;
        case t_STRING:
            if (inCallArgs_) {
                currentExprType_ = promoverTipos(currentExprType_, TipoBase::T_STRING);
            }
            break;
        case t_KEY_TRUE:
        case t_KEY_FALSE:
            if (inCallArgs_) {
                currentExprType_ = promoverTipos(currentExprType_, TipoBase::T_BOOL);
            }
            break;
        default:
            warn("Token inesperado: " + token->getLexeme() + " na posição " + std::to_string(token->getPosition()));
            if (id != t_DELIM_PONTOVIRGULA && id != t_DELIM_CHAVEE && id != t_DELIM_CHAVED) {
                return; // Ignorar e continuar
            }
            break;
        }
    } else {
        return;
    }

    switch (action) {
    case 2: {
        if (!token) return;
        // evita duplicar o mesmo ID na mesma posição
        if (lastDeclaredPos == token->getPosition()) return;
        // CASO 1: estamos dentro da lista de parâmetros da função
        if (g_inParamList) {
            if (tipoAtual.empty()) {
                throw SemanticError("Parâmetro sem tipo declarado", token->getPosition());
            }
            Simbolo p;
            p.tipo = tipoAtual;
            p.nome = token->getLexeme();
            p.usado = false;
            p.inicializado = true; // parâmetro nasce inicializado
            p.modalidade = "parametro";
            p.escopo = g_funcEmConstrucao.empty()
                           ? "global"
                           : g_funcEmConstrucao; // nome da função
            // guarda na lista temporária de parâmetros da função
            g_paramBuffer.push_back(p);
            // e também na tabela global de símbolos (para relatórios, etc.)
            tabelaSimbolo.push_back(p);
            lastDeclaredPos = token->getPosition();
            g_ultimoIdVisto = p.nome;
            g_ultimoIdAntesDaAtrib = g_ultimoIdVisto;
            ultimoDeclaradoNome = g_ultimoIdVisto;
        }
        // CASO 2: declaração "normal" (variável global/local)
        else if (modoDeclaracao) {
            declarar(token);
            lastDeclaredPos = token->getPosition();
            g_ultimoIdVisto = token->getLexeme();
            g_ultimoIdAntesDaAtrib = g_ultimoIdVisto;
            ultimoDeclaradoNome = g_ultimoIdVisto;
        }
        return;
    }
    case 4:
        usar(token);
        return;
    case 3:
        endDeclaracao();
        return;
    case 10: // ID[expr] -> vetor
        if (!ultimoDeclaradoNome.empty())
            marcarUltimoDeclaradoComoVetor(ultimoDeclaradoNome);
        return;
    case 11:
        if (!ultimoDeclaradoNome.empty()) {
            marcarInicializadoPorNome(ultimoDeclaradoNome, pilhaEscopos, tabelaSimbolo);
        }
        return;
    case 12: // ID[...] = { ... }
        if (!ultimoDeclaradoNome.empty())
            marcarInicializadoPorNome(ultimoDeclaradoNome, pilhaEscopos, tabelaSimbolo);
        inInitList = false; initListDepth = 0; pendingInitList = false;
        return;
    case 13: // Marcar inicialização após atribuição
        if (!g_ultimoIdAntesDaAtrib.empty()) {
            if (g_ultimoIdVisto.find('[') != std::string::npos) {
                // Trata atribuição a elemento de vetor (ex.: v[0] = 3)
                std::string nomeVetor = g_ultimoIdAntesDaAtrib;
                marcarElementoVetorInicializado(nomeVetor, -1, pilhaEscopos, tabelaSimbolo);
            } else {
                marcarInicializadoPorNome(g_ultimoIdAntesDaAtrib, pilhaEscopos, tabelaSimbolo);
            }
        }
        return;
    case 20: // ID da chamada de função
        if (token) {
            funcEmChamada_ = token->getLexeme();
            // marca como usado (se não existir, 'usar' já acusa erro)
            usar(token);
        }
        return;
    case 21: // abre parênteses da chamada: zera contagem de args e tipos
        inCallArgs_ = true;
        callArgsCount_ = 0;
        callArgTypes_.clear();
        currentExprType_ = TipoBase::T_DESCONHECIDO;
        return;
    case 23:
        if (inCallArgs_) {
            if (currentExprType_ != TipoBase::T_DESCONHECIDO) {
                ++callArgsCount_;
                callArgTypes_.push_back(currentExprType_);
                std::cerr << "[DEBUG] Fechando argumento #"
                          << callArgsCount_
                          << " com tipo " << tipoBaseToString(currentExprType_) << "\n";
            } else {
                warn("Argumento sem tipo inferido; assumindo como erro ou desconhecido.");
            }
            currentExprType_ = TipoBase::T_DESCONHECIDO;
        }
        break;
    case 22: // fecha chamada: faz verificação
        if (inCallArgs_) {
            inCallArgs_ = false;
            if (!funcEmChamada_.empty()) {
                auto it = funcoes_.find(funcEmChamada_);
                if (it == funcoes_.end()) {
                    error("Chamada à função '" + funcEmChamada_ +
                          "' que não foi declarada como função.");
                } else {
                    const FuncSignature& sig = it->second;
                    std::size_t esperados = sig.paramTypes.size();
                    std::cerr << "[DEBUG] Verificando chamada de '"
                              << funcEmChamada_
                              << "': esperados=" << esperados
                              << ", recebidos=" << callArgsCount_ << "\n";
                    for (std::size_t i = 0; i < callArgTypes_.size(); ++i) {
                        std::cerr << " arg" << (i+1)
                        << " tipo=" << tipoBaseToString(callArgTypes_[i]) << "\n";
                    }
                    // 1) Verifica QUANTIDADE
                    if ((std::size_t)callArgsCount_ != esperados) {
                        error(
                            "Chamada à função '" + funcEmChamada_ +
                            "' com quantidade incorreta de parâmetros. Esperados " +
                            std::to_string(esperados) +
                            ", recebidos " + std::to_string(callArgsCount_) + "."
                            );
                    } else {
                        // 2) Verifica TIPO + ORDEM
                        if (callArgTypes_.size() != esperados) {
                            warn("Número de tipos de argumentos registrados não bate com a quantidade na função '" +
                                 funcEmChamada_ + "'.");
                        } else {
                            for (std::size_t i = 0; i < esperados; ++i) {
                                TipoBase esperadoT = stringToTipoBase(sig.paramTypes[i]);
                                TipoBase recebidoT = callArgTypes_[i];
                                if (!tiposCompativeis(esperadoT, recebidoT)) {
                                    error(
                                        "Tipo incompatível no parâmetro " + std::to_string(i + 1) +
                                        " da função '" + funcEmChamada_ + "'. Esperado '" +
                                        sig.paramTypes[i] + "', recebido '" +
                                        tipoBaseToString(recebidoT) + "'."
                                        );
                                }
                            }
                        }
                    }
                }
            }
            funcEmChamada_.clear();
            callArgsCount_ = 0;
            callArgTypes_.clear();
            currentExprType_ = TipoBase::T_DESCONHECIDO;
        }
        return;
    default:
        break;
    }
}
