#include "CodeGeneratorBIP.h"
#include <fstream>
#include <algorithm>

// =================== internos ===================
static inline bool isIdentChar(unsigned char c) {
    return std::isalnum(c) || c=='_' || c=='$';
}

static bool isIntegerLiteral(const std::string& s) {
    if (s.empty()) return false;

    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') {
        i = 1;
        if (i >= s.size()) return false;
    }

    for (; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
            return false;
        }
    }
    return true;
}

// =================== ctor ===================
CodeGeneratorBIP::CodeGeneratorBIP(const Options& opt)
    : opt_(opt) {}

// =================== helpers estáticos ===================
std::string CodeGeneratorBIP::sanitizeLabel(const std::string& s) {
    std::string r; r.reserve(s.size());
    for (unsigned char c : s) r.push_back(isIdentChar(c) ? char(c) : '_');
    if (r.empty()) r = "sym";
    return r;
}

bool CodeGeneratorBIP::isGlobalDataCandidate(const Simbolo& s) {
    // NÃO entra em .data:
    if (s.modalidade == "funcao" || s.modalidade == "parametro") {
        return false;
    }

    // ENTRA em .data:
    // - variáveis escalares
    if (s.modalidade == "variavel") {
        return true;
    }

    // - vetores (se sua linguagem tiver)
    if (s.modalidade == "vetor" || s.isVetor) {
        return true;
    }

    return false;
}

// =================== .data ===================
std::string CodeGeneratorBIP::buildDataSection(const std::vector<Simbolo>& tabela) const {
    std::vector<const Simbolo*> cand;
    cand.reserve(tabela.size());
    for (const auto& s : tabela)
        if (isGlobalDataCandidate(s)) cand.push_back(&s);

    if (opt_.sortByName) {
        std::sort(cand.begin(), cand.end(),
                  [](const Simbolo* a, const Simbolo* b){ return a->nome < b->nome; });
    }

    std::ostringstream out;
    if (opt_.includeDataHeader) out << ".data\n";

    std::unordered_set<std::string> used;
    for (const Simbolo* ps : cand) {
        const auto& s = *ps;
        std::string label = sanitizeLabel(s.nome);

        int k = 1;
        while (used.count(label)) {
            label = sanitizeLabel(s.nome) + "_" + std::to_string(k++);
        }
        used.insert(label);

        bool ehVetor = (s.modalidade == "vetor" || s.isVetor);

        int N = 1;
        if (ehVetor) {
            // 1) tenta tamanho registrado pelo parser (setArraySize)
            auto itSz = arraySizes_.find(label);
            if (itSz != arraySizes_.end() && itSz->second > 0) {
                N = itSz->second;
            }
            // 2) se não tiver, tenta Simbolo.vetorTam
            else if (s.vetorTam > 0) {
                N = s.vetorTam;
            }
            // 3) fallback: usa quantidade de inicializadores, se tiver
            else {
                auto itArr = arrayInitialValues_.find(label);
                if (itArr != arrayInitialValues_.end() && !itArr->second.empty())
                    N = (int)itArr->second.size();
                else
                    N = 1;
            }
        }

        out << label << " : ";

        if (!ehVetor) {
            // ESCALAR
            auto it = initialValues_.find(label);
            int v = (it != initialValues_.end()) ? it->second : 0;
            out << v;
        } else {
            // VETOR
            auto itArr = arrayInitialValues_.find(label);
            for (int i = 0; i < N; ++i) {
                int v = 0;
                if (itArr != arrayInitialValues_.end() && i < (int)itArr->second.size()) {
                    v = itArr->second[i];
                }
                out << v;
                if (i+1 < N) out << ", ";
            }
        }
        out << "\n";
    }
    out << "__TMP0 : 0\n";

    out << "\n";
    return out.str();
}


bool CodeGeneratorBIP::emitDataToFile(const std::string& outPath,
                                      const std::vector<Simbolo>& tabela,
                                      std::function<void(const std::string&)> logger) const {
    const std::string text = buildDataSection(tabela);
    std::ofstream ofs(outPath, std::ios::binary);
    if (!ofs) {
        if (logger) logger("erro: não foi possível abrir " + outPath + " para escrita");
        return false;
    }
    ofs << text;
    if (!ofs.good()) {
        if (logger) logger("erro: falha ao gravar em " + outPath);
        return false;
    }
    if (logger) logger("gerou seção .data em: " + outPath);
    return true;
}

// =================== .text – API ===================
void CodeGeneratorBIP::clearText() {
    text_.clear();
    labelCounter_ = 0;
}

void CodeGeneratorBIP::emitInstr(const std::string& instr) { text_.push_back(instr); }

void CodeGeneratorBIP::emitLabel(const std::string& label) {
    std::ostringstream oss; oss << sanitizeLabel(label) << ":";
    text_.push_back(oss.str());
}

std::string CodeGeneratorBIP::newLabel(const std::string& prefix) {
    std::ostringstream oss; oss << prefix << (++labelCounter_);
    return oss.str();
}

// globais: LDI nome ; LD/STO k
void CodeGeneratorBIP::emitLoadId(const std::string& nome) {
    std::string lbl = sanitizeLabel(nome);
    emitInstr("LD " + lbl);
}

void CodeGeneratorBIP::emitStoreId(const std::string& nome) {
    std::string lbl = sanitizeLabel(nome);
    emitInstr("STO " + lbl);     // Mem[lbl] <- ACC
}

// vetores com deslocamento constante
void CodeGeneratorBIP::emitLoadIdOffset(const std::string& nome, int k) {
    std::string lbl = sanitizeLabel(nome);

    // índice constante k no $indr
    emitInstr("LDI " + std::to_string(k));
    emitInstr("STO $indr");

    // carrega vetor[$indr] em ACC
    emitInstr("LDV " + lbl);
}

// ACC -> vetor[k]
void CodeGeneratorBIP::emitStoreIdOffset(const std::string& nome, int k) {
    std::string lbl = sanitizeLabel(nome);

    // índice constante k no $indr
    emitInstr("LDI " + std::to_string(k));
    emitInstr("STO $indr");

    // armazena ACC em vetor[$indr]
    emitInstr("STOV " + lbl);
}

// aritmética
void CodeGeneratorBIP::emitAdd() { emitInstr("ADD"); }
void CodeGeneratorBIP::emitSub() { emitInstr("SUB"); }
void CodeGeneratorBIP::emitMul() { emitInstr("MUL"); }
void CodeGeneratorBIP::emitDiv() { emitInstr("DIV"); }

// bit a bit
void CodeGeneratorBIP::emitAnd() { emitInstr("AND"); }
void CodeGeneratorBIP::emitOr()  { emitInstr("OR");  }
void CodeGeneratorBIP::emitXor() { emitInstr("XOR"); }
void CodeGeneratorBIP::emitNot() { emitInstr("NOT"); }
void CodeGeneratorBIP::emitShl() { emitInstr("SHL"); }
void CodeGeneratorBIP::emitShr() { emitInstr("SHR"); }

// desvios
void CodeGeneratorBIP::emitJmp(const std::string& label) {
    emitInstr("JMP " + sanitizeLabel(label));
}
void CodeGeneratorBIP::emitJz(const std::string& label) {
    emitInstr("JZ " + sanitizeLabel(label));
}

// =================== Atribuições ===================
// Simples: variável/vetor ← variável/vetor
void CodeGeneratorBIP::emitAssign(const std::string& dest, bool destIsArray, int destIndex,
                                  const std::string& src,  bool srcIsArray,  int srcIndex)
{
    std::string d = sanitizeLabel(dest);
    std::string s = sanitizeLabel(src);

    // =============== DESTINO ESCALAR ===============
    if (!destIsArray) {
        // x = v[k];
        if (srcIsArray) {
            emitInstr("LDI " + std::to_string(srcIndex));
            emitInstr("STO $indr");
            emitInstr("LDV " + s);      // ACC = v[k]
            emitStoreId(dest);          // x = ACC
            return;
        }

        // x = literal;
        if (isIntegerLiteral(src)) {
            emitInstr("LDI " + src);
            emitStoreId(dest);
            return;
        }

        // x = y;
        emitLoadId(src);
        emitStoreId(dest);
        return;
    }

    // =============== DESTINO VETOR[destIndex] ===============
    // (índice constante passado em destIndex)

    // d[destIndex] = (literal ou escalar)
    if (!srcIsArray) {
        // fixa índice do destino primeiro
        emitInstr("LDI " + std::to_string(destIndex));
        emitInstr("STO $indr");          // $indr = destIndex

        if (isIntegerLiteral(src))
            emitInstr("LDI " + src);     // ACC = literal
        else
            emitLoadId(src);             // ACC = variável

        emitInstr("STOV " + d);          // d[destIndex] = ACC
        return;
    }

    // d[destIndex] = v[srcIndex];  (ambos índices constantes)
    {
        // precisamos de temporário porque usamos $indr duas vezes
        // 1) lê v[srcIndex] em __TMP0
        emitInstr("LDI " + std::to_string(srcIndex));
        emitInstr("STO $indr");
        emitInstr("LDV " + s);               // ACC = v[srcIndex]
        emitInstr("STO __TMP0");             // guarda

        // 2) seta índice de destino
        emitInstr("LDI " + std::to_string(destIndex));
        emitInstr("STO $indr");

        // 3) recarrega valor e grava em d[destIndex]
        emitInstr("LD __TMP0");
        emitInstr("STOV " + d);
        return;
    }

    // casos não tratados aqui: ignorados pelo gerador básico
}

// Vetor com índice variável (v[i] = x)
void CodeGeneratorBIP::emitAssignVarIndex(const std::string& dest,
                                          const std::string& idx,
                                          const std::string& src) {
    std::string lblDest = sanitizeLabel(dest);

    // idx -> $indr
    emitLoadId(idx);            // LD idx
    emitInstr("STO $indr");

    // src -> ACC
    emitLoadId(src);            // LD src

    // ACC -> vetor[$indr]
    emitInstr("STOV " + lblDest);
}


// =================== construção da .text / programa ===================
std::string CodeGeneratorBIP::buildTextSection() const {
    std::ostringstream oss;

    if (opt_.includeTextHeader)
        oss << ".text\n";
    oss << opt_.entryLabel << ":\n";

    for (const auto& l : text_) {
        if (!l.empty() && l.back() == ':')
            oss << l << "\n";
        else
            oss << "    " << l << "\n";
    }
    oss << "    HLT 0\n";
    return oss.str();
}


std::string CodeGeneratorBIP::buildProgram(const std::vector<Simbolo>& tabela) const {
    std::ostringstream oss;
    oss << buildDataSection(tabela);
    oss << buildTextSection();
    return oss.str();
}

void CodeGeneratorBIP::setInitialValue(const std::string& name, int value) {
    std::string lbl = sanitizeLabel(name);
    initialValues_[lbl] = value;
}

void CodeGeneratorBIP::setArrayInitialValues(const std::string& name,
                                             const std::vector<int>& values) {
    std::string lbl = sanitizeLabel(name);
    arrayInitialValues_[lbl] = values;
}

void CodeGeneratorBIP::setArraySize(const std::string& name, int size) {
    if (size <= 0) return;
    std::string lbl = sanitizeLabel(name);
    arraySizes_[lbl] = size;
}

static std::string trimSpaces(const std::string& s) {
    size_t i = 0, j = s.size();
    while (i < j && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    while (j > i && std::isspace(static_cast<unsigned char>(s[j - 1]))) --j;
    return s.substr(i, j - i);
}

// expr no formato nome[idx]
// idx pode ser número inteiro ou identificador
static bool parseArrayAccess(const std::string& expr,
                             std::string& arr,
                             std::string& idx,
                             bool& idxIsLiteral)
{
    std::string s = trimSpaces(expr);
    size_t lb = s.find('[');
    if (lb == std::string::npos) return false;
    size_t rb = s.find(']', lb + 1);
    if (rb == std::string::npos) return false;

    arr = trimSpaces(s.substr(0, lb));
    idx = trimSpaces(s.substr(lb + 1, rb - lb - 1));
    if (arr.empty() || idx.empty()) return false;

    // arr deve ser identificador simples
    if (!(std::isalpha(static_cast<unsigned char>(arr[0])) || arr[0] == '_'))
        return false;
    for (unsigned char c : arr)
        if (!isIdentChar(c)) return false;

    // idx: literal ou identificador
    idxIsLiteral = isIntegerLiteral(idx);
    if (!idxIsLiteral) {
        if (!(std::isalpha(static_cast<unsigned char>(idx[0])) || idx[0] == '_'))
            return false;
        for (unsigned char c : idx)
            if (!isIdentChar(c)) return false;
    }

    return true;
}

// Atribuição com operação simples (a = b + c)
void CodeGeneratorBIP::emitAssignSimpleExpr(const std::string& dest,
                                            const std::string& op1,
                                            const std::string& oper,
                                            const std::string& op2)
{
    // Caso: dest = constante;
    if (oper.empty() && op2.empty() && isIntegerLiteral(op1)) {
        emitInstr("LDI " + op1);   // ACC <- literal
        emitStoreId(dest);         // STO dest
        return;
    }

    // Detecta se op1/op2 são acessos a vetor
    std::string arr1, idx1, arr2, idx2;
    bool idx1IsLit = false, idx2IsLit = false;
    bool op1IsArr = parseArrayAccess(op1, arr1, idx1, idx1IsLit);
    bool op2IsArr = parseArrayAccess(op2, arr2, idx2, idx2IsLit);

    auto isComm = [](const std::string& op) {
        return op == "+" || op == "&" || op == "|" || op == "^";
    };

    // Helper: carrega escalar ou literal em ACC
    auto loadScalarOrLiteral = [this](const std::string& v) {
        if (isIntegerLiteral(v))
            emitInstr("LDI " + v);
        else
            emitLoadId(v);
    };

    // Helper: carrega elemento de vetor em ACC
    auto loadArrayElem = [this](const std::string& arr,
                                const std::string& idx,
                                bool idxIsLit)
    {
        std::string a = sanitizeLabel(arr);
        if (idxIsLit) {
            emitInstr("LDI " + idx);
            emitInstr("STO $indr");
        } else {
            emitLoadId(idx);
            emitInstr("STO $indr");
        }
        emitInstr("LDV " + a);
    };

    // ============ 1) Nenhum é vetor -> comportamento antigo ============
    if (!op1IsArr && !op2IsArr) {
        // Carrega op1
        loadScalarOrLiteral(op1);

        // Aplica operação com op2
        if (oper == "+") {
            if (isIntegerLiteral(op2))
                emitInstr("ADDI " + op2);
            else
                emitInstr("ADD " + op2);
        }
        else if (oper == "-") {
            if (isIntegerLiteral(op2))
                emitInstr("SUBI " + op2);
            else
                emitInstr("SUB " + op2);
        }
        else if (oper == "&") {
            if (isIntegerLiteral(op2))
                emitInstr("ANDI " + op2);
            else
                emitInstr("AND " + op2);
        }
        else if (oper == "|") {
            if (isIntegerLiteral(op2))
                emitInstr("ORI " + op2);
            else
                emitInstr("OR " + op2);
        }
        else if (oper == "^") {
            if (isIntegerLiteral(op2))
                emitInstr("XORI " + op2);
            else
                emitInstr("XOR " + op2);
        }
        else if (oper.empty()) {
            // dest = op1; já está em ACC
        }
        // operador desconhecido -> deixa ACC como está

        emitStoreId(dest);
        return;
    }

    // ============ 2) op1 é vetor, op2 NÃO é vetor ============
    // dest = v[idx] op escalar/literal
    if (op1IsArr && !op2IsArr) {
        loadArrayElem(arr1, idx1, idx1IsLit);   // ACC = v[idx]

        if (oper == "+") {
            if (isIntegerLiteral(op2))
                emitInstr("ADDI " + op2);
            else
                emitInstr("ADD " + op2);
        }
        else if (oper == "-") {
            if (isIntegerLiteral(op2))
                emitInstr("SUBI " + op2);
            else
                emitInstr("SUB " + op2);
        }
        else if (oper == "&") {
            if (isIntegerLiteral(op2))
                emitInstr("ANDI " + op2);
            else
                emitInstr("AND " + op2);
        }
        else if (oper == "|") {
            if (isIntegerLiteral(op2))
                emitInstr("ORI " + op2);
            else
                emitInstr("OR " + op2);
        }
        else if (oper == "^") {
            if (isIntegerLiteral(op2))
                emitInstr("XORI " + op2);
            else
                emitInstr("XOR " + op2);
        }
        // oper vazio não cai aqui (já tratado antes)

        emitStoreId(dest);
        return;
    }

    // ============ 3) op1 NÃO é vetor, op2 é vetor ============
    // dest = escalar/literal op v[idx]
    if (!op1IsArr && op2IsArr) {
        // Casos comutativos: podemos inverter com segurança usando __TMP0
        if (isComm(oper)) {
            // guarda op1 em __TMP0
            loadScalarOrLiteral(op1);      // ACC = op1
            emitInstr("STO __TMP0");

            // ACC = v[idx]
            loadArrayElem(arr2, idx2, idx2IsLit);

            if (oper == "+")
                emitInstr("ADD __TMP0");
            else if (oper == "&")
                emitInstr("AND __TMP0");
            else if (oper == "|")
                emitInstr("OR __TMP0");
            else if (oper == "^")
                emitInstr("XOR __TMP0");

            emitStoreId(dest);
            return;
        }

        // Subtração: dest = op1 - v[idx]
        if (oper == "-") {
            // 1) carrega v[idx] e guarda em __TMP0
            loadArrayElem(arr2, idx2, idx2IsLit);   // ACC = v[idx]
            emitInstr("STO __TMP0");

            // 2) ACC = op1
            loadScalarOrLiteral(op1);

            // 3) ACC = op1 - v[idx]
            emitInstr("SUB __TMP0");

            emitStoreId(dest);
            return;
        }

        // operador não suportado -> ignora
        return;
    }

    // ============ 4) Ambos são vetores ============
    // dest = v1[idx1] op v2[idx2]
    if (op1IsArr && op2IsArr) {
        // Comutativos: usamos __TMP0 como um dos operandos
        if (isComm(oper)) {
            // ACC = v1[idx1], guarda em __TMP0
            loadArrayElem(arr1, idx1, idx1IsLit);
            emitInstr("STO __TMP0");

            // ACC = v2[idx2]
            loadArrayElem(arr2, idx2, idx2IsLit);

            if (oper == "+")
                emitInstr("ADD __TMP0");
            else if (oper == "&")
                emitInstr("AND __TMP0");
            else if (oper == "|")
                emitInstr("OR __TMP0");
            else if (oper == "^")
                emitInstr("XOR __TMP0");

            emitStoreId(dest);
            return;
        }

        // Subtração: dest = v1[idx1] - v2[idx2]
        if (oper == "-") {
            // 1) ACC = v2[idx2]; guarda em __TMP0
            loadArrayElem(arr2, idx2, idx2IsLit);
            emitInstr("STO __TMP0");

            // 2) ACC = v1[idx1]
            loadArrayElem(arr1, idx1, idx1IsLit);

            // 3) ACC = v1[idx1] - v2[idx2]
            emitInstr("SUB __TMP0");

            emitStoreId(dest);
            return;
        }

        // operador não suportado
        return;
    }
}
