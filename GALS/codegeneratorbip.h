#ifndef CODEGENERATOR_BIP_H
#define CODEGENERATOR_BIP_H

#include "Semantico.h"   // precisa do tipo Simbolo

#include <string>
#include <vector>
#include <functional>
#include <unordered_set>
#include <cctype>
#include <sstream>
#include <unordered_map>

class CodeGeneratorBIP {
public:
    struct Options {
        // .data
        bool        includeDataHeader;
        bool        sortByName;
        std::string dataComment;

        // .text
        bool        includeTextHeader;
        std::string entryLabel;      // ex.: "_PRINCIPAL"
        std::string textComment;

        Options()
            : includeDataHeader(true)
            , sortByName(true)
            , dataComment(";")
            , includeTextHeader(true)
            , entryLabel("_PRINCIPAL")
            , textComment(";")
        {}
    };

    explicit CodeGeneratorBIP(const Options& opt = Options());

    // ========= .data =========
    std::string buildDataSection(const std::vector<Simbolo>& tabela) const;

    // ========= .text – API de emissão =========
    void clearText();                               // limpa buffer de texto
    void emitInstr(const std::string& instr);       // emite linha crua
    void emitLabel(const std::string& label);       // rótulo "L1:"
    std::string newLabel(const std::string& prefix ="L"); // gera Lx único

    // Helpers de alto nível (endereços/globais):
    void emitLoadId(const std::string& nome);       // LDI nome ; LD 0
    void emitStoreId(const std::string& nome);      // LDI nome ; STO 0

    // Acesso a vetores (deslocamento constante k):
    void emitLoadIdOffset(const std::string& nome, int k);   // LDI nome ; LD k
    void emitStoreIdOffset(const std::string& nome, int k);  // LDI nome ; STO k

    // Aritmética (topo da pilha / acumulador da BIP):
    void emitAdd();                                 // ADD
    void emitSub();                                 // SUB
    void emitMul();                                 // MUL
    void emitDiv();                                 // DIV

    // Bit a bit:
    void emitAnd();                                 // AND
    void emitOr();                                  // OR
    void emitXor();                                 // XOR
    void emitNot();                                 // NOT
    void emitShl();                                 // SHL
    void emitShr();                                 // SHR

    // Desvios:
    void emitJmp(const std::string& label);         // JMP label
    void emitJz(const std::string& label);          // JZ label

    // ========= Atribuições =========
    void emitAssign(const std::string& dest, bool destIsArray, int destIndex,
                    const std::string& src,  bool srcIsArray,  int srcIndex);

    void emitAssignVarIndex(const std::string& dest, const std::string& idx,
                            const std::string& src);

    void emitAssignSimpleExpr(const std::string& dest,
                              const std::string& op1,
                              const std::string& oper,
                              const std::string& op2);

    // ========= Programa completo =========
    std::string buildTextSection() const;
    std::string buildProgram(const std::vector<Simbolo>& tabela) const;

    // ========= utilitários =========
    bool emitDataToFile(const std::string& outPath,
                        const std::vector<Simbolo>& tabela,
                        std::function<void(const std::string&)> logger = nullptr) const;

    void setInitialValue(const std::string& name, int value);

    void setArrayInitialValues(const std::string& name, const std::vector<int>& values);

    void setArraySize(const std::string& name, int size);

private:
    Options opt_;
    std::vector<std::string> text_;
    mutable int labelCounter_ = 0;

    static bool        isGlobalDataCandidate(const Simbolo& s);

    static std::string sanitizeLabel(const std::string& s);

    std::unordered_map<std::string,int> initialValues_;

    std::unordered_map<std::string, std::vector<int>> arrayInitialValues_;

    std::unordered_map<std::string, int> arraySizes_;
};

#endif // CODEGENERATOR_BIP_H
