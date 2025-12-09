#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QAbstractItemView>
#include <QPlainTextEdit>
#include <QDockWidget>
#include <QRegularExpression>
#include <QFile>
#include <QMap>
#include <QStringList>
#include <sstream>

// GALS
#include "Lexico.h"
#include "Sintatico.h"
#include "Semantico.h"
#include "LexicalError.h"
#include "SyntacticError.h"
#include "SemanticError.h"

// Gerador unificado (.data + .text + buildProgram)
#include "CodeGeneratorBIP.h"

// Contadores globais para geração de labels (loops / ifs)
int g_loopCounter = 0;
int g_ifCounter   = 0;


// preenche a QTableView da Tabela de Símbolos
void MainWindow::preencherTabelaSimbolos(const std::vector<Simbolo>& tabela)
{
    // limpa conteúdo anterior
    modelSimbolos->removeRows(0, modelSimbolos->rowCount());

    // ajusta o número de linhas
    const int n = static_cast<int>(tabela.size());
    modelSimbolos->setRowCount(n);

    // preenche linha a linha
    for (int i = 0; i < n; ++i) {
        const auto& s = tabela[i];

        // Espera-se que Simbolo possua: nome, tipo, modalidade, escopo, usado, inicializado
        modelSimbolos->setItem(i, 0, new QStandardItem(QString::fromStdString(s.nome)));
        modelSimbolos->setItem(i, 1, new QStandardItem(QString::fromStdString(s.tipo)));
        modelSimbolos->setItem(i, 2, new QStandardItem(QString::fromStdString(s.modalidade)));
        modelSimbolos->setItem(i, 3, new QStandardItem(QString::fromStdString(s.escopo)));
        modelSimbolos->setItem(i, 4, new QStandardItem(s.usado ? "sim" : "não"));
        modelSimbolos->setItem(i, 5, new QStandardItem(s.inicializado ? "sim" : "não"));
    }

    ui->tableView->resizeColumnsToContents();
}

// Mapa global (estático) nomeFunc -> [param1, param2, ...]
static QMap<QString, QStringList> s_funcParams;

// Nome da função atual em que o emissor está gerando código
static QString s_currentFunctionName;

// marca se a função (por nome) teve algum "return" no corpo
static QMap<QString, bool> s_funcHasReturn;

// Helper: gera .text a partir do texto do editor
static void emitirTextBasico(CodeGeneratorBIP& gen, const QString& fonteEditor)
{
    // Regex básicos reutilizados
    QRegularExpression rxCin(
        R"(^\s*(?:cin|Cin|CIN)\s*>>\s*([^;]+);)"
        );
    QRegularExpression rxCout(
        R"(^\s*(?:cout|Cout|COUT)\s*<<\s*(.+);)"
        );

    // atribuição com destino escalar
    QRegularExpression rxAssign(
        R"(^\s*([A-Za-z_]\w*)\s*=\s*(.+);)"
        );

    // atribuição com destino vetor: v[algo] = ...
    QRegularExpression rxAssignArr(
        R"(^\s*([A-Za-z_]\w*)\s*\[\s*(.+)\s*\]\s*=\s*(.+);)"
        );

    // binário (fallback)
    QRegularExpression rxBin(
        R"(^([A-Za-z_]\w*(?:\[\s*(?:[A-Za-z_]\w*|\d+)\s*\])?|[+\-]?\d+)\s*([+\-\*/\&\|\^])\s*([A-Za-z_]\w*(?:\[\s*(?:[A-Za-z_]\w*|\d+)\s*\])?|[+\-]?\d+)$)"
        );

    QRegularExpression rxLit(R"(^[+\-]?\d+$)");
    QRegularExpression rxId (R"(^[A-Za-z_]\w*$)");

    // declaração escalar com init: int a = 10;
    QRegularExpression rxDeclInit(
        R"(^\s*(int|INT)\s+([A-Za-z_]\w*)\s*=\s*([+\-]?\d+)\s*;)"
        );

    // vetores
    QRegularExpression rxArrayDecl(
        R"(^\s*(int|INT)\s+([A-Za-z_]\w*)\s*\[\s*(\d+)\s*\]\s*;)"
        );

    QRegularExpression rxArrayInit(
        R"(^\s*(int|INT)\s+([A-Za-z_]\w*)\s*\[\s*(\d+)\s*\]\s*=\s*\{([^}]*)\}\s*;)"
        );

    // usos tipo d[2], d[i]
    QRegularExpression rxArrIdxConst(
        R"(^([A-Za-z_]\w*)\s*\[\s*(\d+)\s*\]$)"
        );
    QRegularExpression rxArrIdxVar(
        R"(^([A-Za-z_]\w*)\s*\[\s*([A-Za-z_]\w*)\s*\]$)"
        );

    // while (x) comando;
    QRegularExpression rxWhileSimple(
        R"(^\s*while\s*\(\s*([A-Za-z_]\w*)\s*\)\s*(.+);$)"
        );
    // while (x) {
    QRegularExpression rxWhileBlock(
        R"(^\s*while\s*\(\s*([A-Za-z_]\w*)\s*\)\s*\{\s*$)"
        );

    // if (expr1 op expr2) comando;
    QRegularExpression rxIfSimple(
        R"(^\s*if\s*\(\s*(.+?)\s*(==|!=|>=|<=|>|<)\s*(.+?)\s*\)\s*(.+);$)"
        );

    // if (expr1 op expr2) {
    QRegularExpression rxIfBlock(
        R"(^\s*if\s*\(\s*(.+?)\s*(==|!=|>=|<=|>|<)\s*(.+?)\s*\)\s*\{\s*$)"
        );

    // else {
    QRegularExpression rxElseBlock(
        R"(^\s*else\s*\{\s*$)"
        );
    // else comando;
    QRegularExpression rxElseSimple(
        R"(^\s*else\s*(.+);$)"
        );

    // while (expr1 op expr2) { ... }
    QRegularExpression rxWhileRelBlock(
        R"(^\s*while\s*\(\s*(.+?)\s*(==|!=|>=|<=|>|<)\s*(.+?)\s*\)\s*\{\s*$)"
        );

    // while (expr1 op expr2) comando;
    QRegularExpression rxWhileRelSimple(
        R"(^\s*while\s*\(\s*(.+?)\s*(==|!=|>=|<=|>|<)\s*(.+?)\s*\)\s*(.+);$)"
        );

    // do { ... } while (x);
    QRegularExpression rxDoBlock(
        R"(^\s*do\s*\{\s*$)"
        );

    // } while (expr1 op expr2);
    QRegularExpression rxDoWhileRel(
        R"(^\}\s*while\s*\(\s*(.+?)\s*(==|!=|>=|<=|>|<)\s*(.+?)\s*\)\s*;)"
        );

    // } while (x);
    QRegularExpression rxDoWhileVar(
        R"(^\}\s*while\s*\(\s*([A-Za-z_]\w*)\s*\)\s*;)"
        );

    // for (init; cond; incr) { ... }
    QRegularExpression rxForBlock(
        R"(^\s*for\s*\(\s*(.+?)\s*;\s*(.+?)\s*;\s*(.+?)\s*\)\s*\{\s*$)"
        );

    // for (init; cond; incr) comando;
    QRegularExpression rxForSimple(
        R"(^\s*for\s*\(\s*(.+?)\s*;\s*(.+?)\s*;\s*(.+?)\s*\)\s*(.+);$)"
        );

    // condição genérica relacional: a op b
    QRegularExpression rxRelCond(
        R"(^\s*(.+?)\s*(==|!=|>=|<=|>|<)\s*(.+?)\s*$)"
        );

    // Definição: (void|int) foo(int a, int b) { ... } ou (void|int) foo() { ... }
    QRegularExpression rxFuncDefFunc(
        R"(^\s*(void|VOID|int|INT)\s+([A-Za-z_]\w*)\s*\(\s*(.*?)\s*\)\s*\{\s*$)"
        );

    // Chamada: foo(a, b); ou foo();
    QRegularExpression rxFuncCall(
        R"(^\s*([A-Za-z_]\w*)\s*\(\s*(.*?)\s*\)\s*;)"
        );

    // Chamada de função como expressão: foo(a, b)   (sem ;)
    QRegularExpression rxFuncCallExpr(
        R"(^\s*([A-Za-z_]\w*)\s*\(\s*(.*?)\s*\)\s*$)"
        );

    // return;
    QRegularExpression rxReturnVoid(
        R"(^\s*return\s*;\s*$)"
        );

    // return com expressão: return x; ou return (x + y);
    QRegularExpression rxReturnExpr(
        R"(^\s*return\s*(?:\(\s*(.+?)\s*\)|(.+?))\s*;\s*$)"
        );

    // se "id" for parâmetro da função atual, renomeia para "Func_id"
    auto mangleIdIfParam = [&](const std::string& id) -> std::string {
        if (s_currentFunctionName.isEmpty())
            return id;

        QString func   = s_currentFunctionName;
        QStringList ps = s_funcParams.value(func);
        QString idQ    = QString::fromStdString(id);

        if (ps.contains(idQ)) {
            return (func + "_" + idQ).toStdString();
        }
        return id;
    };

    // gera chamada de função com passagem por cópia
    auto gerarChamadaFuncao = [&](const QString& funcNameQ,
                                  const QStringList& argExprs) {
        std::string nomeFunc = funcNameQ.toStdString();
        if (nomeFunc == "main") {
            return; // não faz CALL main
        }

        // Recupera lista de parâmetros formais ORIGINAIS: "x", "y"...
        QStringList paramNames = s_funcParams.value(funcNameQ);

        int paramCount = paramNames.size();
        int argCount   = argExprs.size();
        int count      = std::min(paramCount, argCount);

        for (int idx = 0; idx < count; ++idx) {
            QString paramNameQ = paramNames[idx];          // ex: "x"
            QString argExprQ   = argExprs[idx].trimmed();  // ex: "a+1"

            if (paramNameQ.isEmpty() || argExprQ.isEmpty())
                continue;

            // COISA_x = <expr do chamador>;
            QString atrib = funcNameQ + "_" + paramNameQ + " = " + argExprQ + ";";

            emitirTextBasico(gen, atrib);
        }

        std::string labelFunc = "FUNC_" + nomeFunc;
        gen.emitInstr("CALL " + labelFunc);
    };

    // Helpers para N-árias
    auto tokenizeExpr = [](const QString& rhs,
                           QStringList& terms,
                           QStringList& ops) -> bool
    {
        QString tok;
        int depth = 0;
        auto flushTok = [&](){
            QString t = tok.trimmed();
            if (!t.isEmpty()) terms.push_back(t);
            tok.clear();
        };

        for (int i = 0; i < rhs.size(); ++i) {
            const QChar ch = rhs[i];

            if (ch == '[') { depth++; tok += ch; continue; }
            if (ch == ']') { depth = qMax(0, depth-1); tok += ch; continue; }

            if (depth == 0 && QString("+-*/&|^").contains(ch)) {
                bool atStartOrAfterOp =
                    tok.trimmed().isEmpty() && (terms.isEmpty() || (!ops.isEmpty() && terms.size() == ops.size()));
                if (atStartOrAfterOp && (ch == '+' || ch == '-')) {
                    tok += ch;
                } else {
                    flushTok();
                    ops.push_back(QString(ch));
                }
            } else {
                tok += ch;
            }
        }
        flushTok();

        if (!ops.isEmpty() && ops.size() + 1 != terms.size())
            return false;

        return !terms.isEmpty();
    };

    auto emitAssignNaryToDest = [&](const std::string& dest,
                                    const QString& rhsQ) -> bool
    {
        QStringList terms, ops;
        if (!tokenizeExpr(rhsQ, terms, ops)) return false;

        // Caso 1 termo
        if (ops.isEmpty()) {
            const QString t = terms[0].trimmed();

            // literal
            if (rxLit.match(t).hasMatch()) {
                gen.emitAssignSimpleExpr(dest, t.toStdString(), "", "");
                return true;
            }

            // ID simples (pode ser parâmetro)
            if (rxId.match(t).hasMatch()) {
                std::string id = t.toStdString();
                id = mangleIdIfParam(id);
                gen.emitAssign(dest, false, 0, id, false, 0);
                return true;
            }

            // arr[NUM]
            auto rc = rxArrIdxConst.match(t);
            if (rc.hasMatch()) {
                std::string arr = rc.captured(1).toStdString();
                int idx         = rc.captured(2).toInt();
                gen.emitLoadIdOffset(arr, idx);
                gen.emitStoreId(dest);
                return true;
            }

            // arr[i]
            auto rv = rxArrIdxVar.match(t);
            if (rv.hasMatch()) {
                std::string arr = rv.captured(1).toStdString();
                std::string i   = rv.captured(2).toStdString();
                i = mangleIdIfParam(i);
                gen.emitLoadId(i);
                gen.emitInstr("STO $indr");
                gen.emitInstr("LDV " + arr);
                gen.emitStoreId(dest);
                return true;
            }

            return false;
        }

        // >= 2 termos: dobra à esquerda no __TMP0
        std::string acc = "__TMP0";

        auto termToStd = [&](const QString& qq) -> std::string {
            QString qtrim = qq.trimmed();
            std::string s = qtrim.toStdString();
            if (rxId.match(qtrim).hasMatch()) {
                s = mangleIdIfParam(s);
            }
            // índices de vetor são tratados por CodeGeneratorBIP
            return s;
        };

        gen.emitAssignSimpleExpr(
            acc,
            termToStd(terms[0]),
            ops[0].trimmed().toStdString(),
            termToStd(terms[1])
            );

        for (int k = 1; k < ops.size(); ++k) {
            gen.emitAssignSimpleExpr(
                acc,
                acc,
                ops[k].trimmed().toStdString(),
                termToStd(terms[k+1])
                );
        }

        if (dest != acc) {
            gen.emitAssign(dest, false, 0, acc, false, 0);
        }
        return true;
    };

    auto evalToTmp0 = [&](const QString& rhsQ) -> bool {
        return emitAssignNaryToDest("__TMP0", rhsQ);
    };

    // Helpers para carregar termos simples (ID, literal, v[NUM], v[i])
    auto storeSimpleTermToTmp0 = [&](const QString& termQ) -> bool {
        QString t = termQ.trimmed();
        if (t.isEmpty()) return false;

        // literal
        if (rxLit.match(t).hasMatch()) {
            gen.emitInstr("LDI " + t.toStdString());
            gen.emitInstr("STO __TMP0");
            return true;
        }

        // ID simples
        if (rxId.match(t).hasMatch()) {
            std::string id = t.toStdString();
            id = mangleIdIfParam(id);
            gen.emitLoadId(id);
            gen.emitInstr("STO __TMP0");
            return true;
        }

        // v[NUM]
        auto mc = rxArrIdxConst.match(t);
        if (mc.hasMatch()) {
            std::string arr = mc.captured(1).toStdString();
            int idx         = mc.captured(2).toInt();
            gen.emitInstr("LDI " + std::to_string(idx));
            gen.emitInstr("STO $indr");
            gen.emitInstr("LDV " + arr);
            gen.emitInstr("STO __TMP0");
            return true;
        }

        // v[i]
        auto mv = rxArrIdxVar.match(t);
        if (mv.hasMatch()) {
            std::string arr = mv.captured(1).toStdString();
            std::string idx = mv.captured(2).toStdString();
            idx = mangleIdIfParam(idx);
            gen.emitLoadId(idx);
            gen.emitInstr("STO $indr");
            gen.emitInstr("LDV " + arr);
            gen.emitInstr("STO __TMP0");
            return true;
        }

        // expressão geral
        return evalToTmp0(t);
    };

    auto loadSimpleTermToAcc = [&](const QString& termQ) -> bool {
        QString t = termQ.trimmed();
        if (t.isEmpty()) return false;

        // literal
        if (rxLit.match(t).hasMatch()) {
            gen.emitInstr("LDI " + t.toStdString());
            return true;
        }

        // ID simples
        if (rxId.match(t).hasMatch()) {
            std::string id = t.toStdString();
            id = mangleIdIfParam(id);
            gen.emitLoadId(id);
            return true;
        }

        // v[NUM]
        auto mc = rxArrIdxConst.match(t);
        if (mc.hasMatch()) {
            std::string arr = mc.captured(1).toStdString();
            int idx         = mc.captured(2).toInt();
            gen.emitInstr("LDI " + std::to_string(idx));
            gen.emitInstr("STO $indr");
            gen.emitInstr("LDV " + arr);
            return true;
        }

        // v[i]
        auto mv = rxArrIdxVar.match(t);
        if (mv.hasMatch()) {
            std::string arr = mv.captured(1).toStdString();
            std::string idx = mv.captured(2).toStdString();
            idx = mangleIdIfParam(idx);
            gen.emitLoadId(idx);
            gen.emitInstr("STO $indr");
            gen.emitInstr("LDV " + arr);
            return true;
        }

        if (!evalToTmp0(t)) return false;
        gen.emitInstr("LD __TMP0");
        return true;
    };

    // gerar salto do IF quando condição for falsa
    auto gerarSaltoIfFalse = [&](const QString& lhsQ,
                                 const QString& rhsQ,
                                 const std::string& op,
                                 const std::string& rotulo) -> bool
    {
        if (!storeSimpleTermToTmp0(rhsQ)) return false;
        if (!loadSimpleTermToAcc(lhsQ))  return false;

        gen.emitInstr("SUB __TMP0");

        if      (op == ">")  gen.emitInstr("BLE " + rotulo);
        else if (op == "<")  gen.emitInstr("BGE " + rotulo);
        else if (op == ">=") gen.emitInstr("BLT " + rotulo);
        else if (op == "<=") gen.emitInstr("BGT " + rotulo);
        else if (op == "==") gen.emitInstr("BNE " + rotulo);
        else if (op == "!=") gen.emitInstr("BEQ " + rotulo);
        else return false;

        return true;
    };

    // condição do for
    auto gerarSaltoForFalse = [&](const QString& condQ,
                                  const std::string& rotuloFalso) -> void
    {
        QString c = condQ.trimmed();
        if (c.isEmpty()) {
            // for(;;)
            return;
        }

        auto mr = rxRelCond.match(c);
        if (mr.hasMatch()) {
            QString lhsQ    = mr.captured(1).trimmed();
            std::string op  = mr.captured(2).toStdString();
            QString rhsQ    = mr.captured(3).trimmed();
            gerarSaltoIfFalse(lhsQ, rhsQ, op, rotuloFalso);
            return;
        }

        // cond simples: variável
        if (rxId.match(c).hasMatch()) {
            std::string condVar = c.toStdString();
            condVar = mangleIdIfParam(condVar);
            gen.emitLoadId(condVar);
            gen.emitInstr("JZ " + rotuloFalso);
            return;
        }

        // literal
        if (rxLit.match(c).hasMatch()) {
            gen.emitInstr("LDI " + c.toStdString());
            gen.emitInstr("JZ " + rotuloFalso);
            return;
        }

        // expressão geral
        if (evalToTmp0(c)) {
            gen.emitInstr("LD __TMP0");
            gen.emitInstr("JZ " + rotuloFalso);
        }
    };

    // Normalização: "} else" -> "}\nelse"
    QString fonteNorm = fonteEditor;
    fonteNorm.replace(QRegularExpression("\\}\\s*else"), "}\nelse");

    QStringList linhas = fonteNorm.split(QRegularExpression("[\r\n]+"),
                                         Qt::SkipEmptyParts);

    // captura corpo de um bloco {...}
    auto capturarBlocoEntreChaves = [&](int headerIndex,
                                        QStringList& bodyLinesOut) -> int
    {
        if (headerIndex < 0 || headerIndex >= linhas.size())
            return -1;

        int braceDepth   = 0;
        int closingIndex = -1;

        QString headerRaw = linhas[headerIndex];
        braceDepth += headerRaw.count('{');
        braceDepth -= headerRaw.count('}');

        for (int j = headerIndex + 1; j < linhas.size(); ++j) {
            QString innerRaw = linhas[j];

            int abre  = innerRaw.count('{');
            int fecha = innerRaw.count('}');

            braceDepth += abre;
            braceDepth -= fecha;

            if (braceDepth < 0) {
                break;
            }

            if (braceDepth == 0) {
                closingIndex = j;
                break;
            }

            bodyLinesOut << innerRaw;
        }

        return closingIndex;
    };

    for (int i = 0; i < linhas.size(); ++i) {
        QString rawLine = linhas[i];
        QString line = rawLine.trimmed();
        if (line.isEmpty())        continue;
        if (line.startsWith("//")) continue;

        if (line == "{" || line == "}")
            continue;

        // RETURN com expressão
        {
            auto mr = rxReturnExpr.match(line);
            if (mr.hasMatch()) {
                QString exprQ;
                if (!mr.captured(1).isEmpty())
                    exprQ = mr.captured(1).trimmed();
                else
                    exprQ = mr.captured(2).trimmed();

                if (!exprQ.isEmpty()) {
                    loadSimpleTermToAcc(exprQ);
                }

                // MARCA: esta função tem pelo menos um return
                if (!s_currentFunctionName.isEmpty()) {
                    s_funcHasReturn[s_currentFunctionName] = true;
                }

                gen.emitInstr("RETURN 0");
                continue;
            }
        }

        // RETURN simples
        {
            auto mr = rxReturnVoid.match(line);
            if (mr.hasMatch()) {

                if (!s_currentFunctionName.isEmpty()) {
                    s_funcHasReturn[s_currentFunctionName] = true;
                }

                gen.emitInstr("RET");
                continue;
            }
        }

        // Definição de função
        {
            auto mf = rxFuncDefFunc.match(line);
            if (mf.hasMatch()) {
                QString tipoQ      = mf.captured(1).trimmed();
                QString nomeQ      = mf.captured(2).trimmed();
                QString paramsQ    = mf.captured(3).trimmed();
                std::string nomeFunc = nomeQ.toStdString();

                s_funcHasReturn[nomeQ] = false;

                QStringList paramNames;
                if (!paramsQ.isEmpty()) {
                    QStringList params = paramsQ.split(
                        QRegularExpression(R"(\s*,\s*)"),
                        Qt::SkipEmptyParts
                        );
                    for (const QString& p : params) {
                        QStringList parts = p.trimmed().split(
                            QRegularExpression(R"(\s+)"),
                            Qt::SkipEmptyParts
                            );
                        if (!parts.isEmpty()) {
                            QString nomeParam = parts.last();
                            paramNames << nomeParam;
                        }
                    }
                }

                s_funcParams.insert(nomeQ, paramNames);

                QStringList bodyLines;
                int closingIndex = capturarBlocoEntreChaves(i, bodyLines);
                if (closingIndex == -1) {
                    continue;
                }

                QString bodyText = bodyLines.join("\n");

                QString prevFunc = s_currentFunctionName;
                s_currentFunctionName = nomeQ;

                if (nomeFunc == "main") {
                    gen.emitInstr("MAIN:");

                    if (!bodyText.trimmed().isEmpty()) {
                        emitirTextBasico(gen, bodyText);
                    }

                    if (!s_funcHasReturn.value(nomeQ, false)) {
                        gen.emitInstr("RETURN 0");
                    }

                    s_currentFunctionName = prevFunc;
                    i = closingIndex;
                    continue;
                }

                std::string labelFunc = "FUNC_" + nomeFunc;
                gen.emitInstr(labelFunc + ":");

                if (!bodyText.trimmed().isEmpty()) {
                    emitirTextBasico(gen, bodyText);
                }

                if (!s_funcHasReturn.value(nomeQ, false)) {
                    gen.emitInstr("RETURN 0");
                }

                s_currentFunctionName = prevFunc;
                i = closingIndex;
                continue;
            }
        }

        // Chamada de função foo(...);
        {
            auto mc = rxFuncCall.match(line);
            if (mc.hasMatch()) {
                QString funcNameQ  = mc.captured(1).trimmed();
                QString argsQ      = mc.captured(2).trimmed();

                QStringList argExprs;
                if (!argsQ.isEmpty()) {
                    argExprs = argsQ.split(
                        QRegularExpression(R"(\s*,\s*)"),
                        Qt::SkipEmptyParts
                        );
                }

                gerarChamadaFuncao(funcNameQ, argExprs);
                continue;
            }
        }

        // FOR bloco
        {
            auto mf = rxForBlock.match(line);
            if (mf.hasMatch()) {
                QString initQ = mf.captured(1).trimmed();
                QString condQ = mf.captured(2).trimmed();
                QString incrQ = mf.captured(3).trimmed();

                int loopId = g_loopCounter++;
                std::string labelBegin = "FOR"    + std::to_string(loopId);
                std::string labelEnd   = "ENDFOR" + std::to_string(loopId);

                if (!initQ.isEmpty())
                    emitirTextBasico(gen, initQ + ";");

                gen.emitInstr(labelBegin + ":");
                gerarSaltoForFalse(condQ, labelEnd);

                QStringList bodyLines;
                int closingIndex = capturarBlocoEntreChaves(i, bodyLines);
                if (closingIndex == -1) {
                    continue;
                }

                QString bodyText = bodyLines.join("\n");
                if (!bodyText.trimmed().isEmpty())
                    emitirTextBasico(gen, bodyText);

                if (!incrQ.isEmpty())
                    emitirTextBasico(gen, incrQ + ";");

                gen.emitInstr("JMP " + labelBegin);
                gen.emitInstr(labelEnd + ":");
                i = closingIndex;
                continue;
            }
        }

        // FOR simples
        {
            auto mf = rxForSimple.match(line);
            if (mf.hasMatch()) {
                QString initQ = mf.captured(1).trimmed();
                QString condQ = mf.captured(2).trimmed();
                QString incrQ = mf.captured(3).trimmed();
                QString bodyQ = mf.captured(4).trimmed();

                int loopId = g_loopCounter++;
                std::string labelBegin = "FOR"    + std::to_string(loopId);
                std::string labelEnd   = "ENDFOR" + std::to_string(loopId);

                if (!initQ.isEmpty())
                    emitirTextBasico(gen, initQ + ";");

                gen.emitInstr(labelBegin + ":");
                gerarSaltoForFalse(condQ, labelEnd);

                if (!bodyQ.isEmpty())
                    emitirTextBasico(gen, bodyQ + ";");

                if (!incrQ.isEmpty())
                    emitirTextBasico(gen, incrQ + ";");

                gen.emitInstr("JMP " + labelBegin);
                gen.emitInstr(labelEnd + ":");
                continue;
            }
        }

        // DO { ... } WHILE (...)
        {
            auto md = rxDoBlock.match(line);
            if (md.hasMatch()) {
                int loopId = g_loopCounter++;
                std::string labelBegin = "DO"    + std::to_string(loopId);
                std::string labelEnd   = "ENDDO" + std::to_string(loopId);

                gen.emitInstr(labelBegin + ":");

                QStringList bodyLines;
                int closingIndex = capturarBlocoEntreChaves(i, bodyLines);
                if (closingIndex == -1) {
                    continue;
                }

                QString bodyText = bodyLines.join("\n");
                if (!bodyText.trimmed().isEmpty()) {
                    emitirTextBasico(gen, bodyText);
                }

                QString condLineTrim = linhas[closingIndex].trimmed();

                auto mr = rxDoWhileRel.match(condLineTrim);
                if (mr.hasMatch()) {
                    QString     lhsQ = mr.captured(1).trimmed();
                    std::string op   = mr.captured(2).toStdString();
                    QString     rhsQ = mr.captured(3).trimmed();

                    if (!gerarSaltoIfFalse(lhsQ, rhsQ, op, labelEnd)) {
                        i = closingIndex;
                        continue;
                    }

                    gen.emitInstr("JMP " + labelBegin);
                    gen.emitInstr(labelEnd + ":");

                    i = closingIndex;
                    continue;
                }

                auto mv = rxDoWhileVar.match(condLineTrim);
                if (mv.hasMatch()) {
                    std::string condVar = mv.captured(1).toStdString();
                    condVar = mangleIdIfParam(condVar);

                    gen.emitLoadId(condVar);
                    gen.emitInstr("JZ " + labelEnd);
                    gen.emitInstr("JMP " + labelBegin);
                    gen.emitInstr(labelEnd + ":");

                    i = closingIndex;
                    continue;
                }

                gen.emitInstr(labelEnd + ":");
                i = closingIndex;
                continue;
            }
        }

        // WHILE (x) { ... }
        {
            auto mw = rxWhileBlock.match(line);
            if (mw.hasMatch()) {
                std::string condVar = mw.captured(1).toStdString();
                condVar = mangleIdIfParam(condVar);

                int loopId = g_loopCounter++;
                std::string labelBegin = "_WHILE_"    + std::to_string(loopId);
                std::string labelEnd   = "_ENDWHILE_" + std::to_string(loopId);

                gen.emitInstr(labelBegin + ":");
                gen.emitLoadId(condVar);
                gen.emitInstr("JZ " + labelEnd);

                QStringList bodyLines;
                int closingIndex = capturarBlocoEntreChaves(i, bodyLines);
                if (closingIndex == -1) {
                    continue;
                }

                QString bodyText = bodyLines.join("\n");
                if (!bodyText.trimmed().isEmpty()) {
                    emitirTextBasico(gen, bodyText);
                }

                gen.emitInstr("JMP " + labelBegin);
                gen.emitInstr(labelEnd + ":");
                i = closingIndex;
                continue;
            }
        }

        // WHILE relacional com bloco
        {
            auto mw = rxWhileRelBlock.match(line);
            if (mw.hasMatch()) {
                QString     lhsQ = mw.captured(1).trimmed();
                std::string op   = mw.captured(2).toStdString();
                QString     rhsQ = mw.captured(3).trimmed();

                int loopId = g_loopCounter++;
                std::string labelBegin = "WHILE"    + std::to_string(loopId);
                std::string labelEnd   = "ENDWHILE" + std::to_string(loopId);

                gen.emitInstr(labelBegin + ":");
                gerarSaltoIfFalse(lhsQ, rhsQ, op, labelEnd);

                QStringList bodyLines;
                int closingIndex = capturarBlocoEntreChaves(i, bodyLines);
                if (closingIndex == -1) {
                    continue;
                }

                QString bodyText = bodyLines.join("\n");
                if (!bodyText.trimmed().isEmpty())
                    emitirTextBasico(gen, bodyText);

                gen.emitInstr("JMP " + labelBegin);
                gen.emitInstr(labelEnd + ":");
                i = closingIndex;
                continue;
            }
        }

        // WHILE simples: while (x) comando;
        {
            auto mw = rxWhileSimple.match(line);
            if (mw.hasMatch()) {
                std::string condVar = mw.captured(1).toStdString();
                condVar = mangleIdIfParam(condVar);
                QString bodyStmtQ   = mw.captured(2).trimmed();

                int loopId = g_loopCounter++;
                std::string labelBegin = "_WHILE_"    + std::to_string(loopId);
                std::string labelEnd   = "_ENDWHILE_" + std::to_string(loopId);

                gen.emitInstr(labelBegin + ":");
                gen.emitLoadId(condVar);
                gen.emitInstr("JZ " + labelEnd);

                QString pseudoFonte = bodyStmtQ + ";";
                emitirTextBasico(gen, pseudoFonte);

                gen.emitInstr("JMP " + labelBegin);
                gen.emitInstr(labelEnd + ":");
                continue;
            }
        }

        // WHILE relacional simples
        {
            auto mw = rxWhileRelSimple.match(line);
            if (mw.hasMatch()) {
                QString lhsQ      = mw.captured(1).trimmed();
                std::string op    = mw.captured(2).toStdString();
                QString rhsQ      = mw.captured(3).trimmed();
                QString bodyStmtQ = mw.captured(4).trimmed();

                int loopId = g_loopCounter++;
                std::string labelBegin = "WHILE"    + std::to_string(loopId);
                std::string labelEnd   = "ENDWHILE" + std::to_string(loopId);

                gen.emitInstr(labelBegin + ":");
                gerarSaltoIfFalse(lhsQ, rhsQ, op, labelEnd);

                QString pseudoFonte = bodyStmtQ + ";";
                emitirTextBasico(gen, pseudoFonte);

                gen.emitInstr("JMP " + labelBegin);
                gen.emitInstr(labelEnd + ":");
                continue;
            }
        }

        // IF com bloco (com ou sem else)
        {
            auto mi = rxIfBlock.match(line);
            if (mi.hasMatch()) {
                QString     lhsQ = mi.captured(1).trimmed();
                std::string op   = mi.captured(2).toStdString();
                QString     rhsQ = mi.captured(3).trimmed();

                int ifId = g_ifCounter++;
                std::string elseLabel = "_ELSE_IF_" + std::to_string(ifId);
                std::string endLabel  = "_END_IF_"  + std::to_string(ifId);

                int headerIndex = i;

                QStringList thenLines;
                int closingIfIndex = capturarBlocoEntreChaves(headerIndex, thenLines);
                if (closingIfIndex == -1) {
                    continue;
                }

                bool hasElse     = false;
                bool elseIsBlock = false;
                int  elseIndex   = -1;

                int k = closingIfIndex + 1;
                while (k < linhas.size()) {
                    QString eRaw  = linhas[k];
                    QString eTrim = eRaw.trimmed();
                    if (eTrim.isEmpty() || eTrim.startsWith("//")) {
                        ++k;
                        continue;
                    }

                    auto mElseBlock = rxElseBlock.match(eTrim);
                    if (mElseBlock.hasMatch()) {
                        hasElse     = true;
                        elseIsBlock = true;
                        elseIndex   = k;
                        break;
                    }

                    auto mElseSimple = rxElseSimple.match(eTrim);
                    if (mElseSimple.hasMatch()) {
                        hasElse     = true;
                        elseIsBlock = false;
                        elseIndex   = k;
                        break;
                    }

                    break;
                }

                if (hasElse) {
                    if (!gerarSaltoIfFalse(lhsQ, rhsQ, op, elseLabel)) {
                        continue;
                    }
                } else {
                    if (!gerarSaltoIfFalse(lhsQ, rhsQ, op, endLabel)) {
                        continue;
                    }
                }

                QString thenText = thenLines.join("\n");
                if (!thenText.trimmed().isEmpty()) {
                    emitirTextBasico(gen, thenText);
                }

                if (hasElse) {
                    gen.emitInstr("JMP " + endLabel);
                    gen.emitInstr(elseLabel + ":");

                    int newI = closingIfIndex;

                    if (elseIsBlock) {
                        QStringList elseLines;
                        int closingElseIndex = capturarBlocoEntreChaves(elseIndex, elseLines);

                        QString elseText = elseLines.join("\n");
                        if (!elseText.trimmed().isEmpty()) {
                            emitirTextBasico(gen, elseText);
                        }

                        if (closingElseIndex != -1)
                            newI = closingElseIndex;
                    } else {
                        auto mElseSimple2 = rxElseSimple.match(linhas[elseIndex].trimmed());
                        if (mElseSimple2.hasMatch()) {
                            QString bodyElse = mElseSimple2.captured(1).trimmed();
                            if (!bodyElse.isEmpty()) {
                                emitirTextBasico(gen, bodyElse + ";");
                            }
                        }
                        newI = elseIndex;
                    }

                    i = newI;
                    gen.emitInstr(endLabel + ":");
                    continue;
                } else {
                    gen.emitInstr(endLabel + ":");
                    i = closingIfIndex;
                    continue;
                }
            }
        }

        // IF simples: if (a op b) comando;
        {
            auto mi = rxIfSimple.match(line);
            if (mi.hasMatch()) {
                QString lhsQ    = mi.captured(1).trimmed();
                std::string op  = mi.captured(2).toStdString();
                QString rhsQ    = mi.captured(3).trimmed();
                QString bodyQ   = mi.captured(4).trimmed();

                int ifId = g_ifCounter++;
                std::string endLabel = "_END_IF_" + std::to_string(ifId);

                if (!gerarSaltoIfFalse(lhsQ, rhsQ, op, endLabel)) {
                    continue;
                }

                QString pseudo = bodyQ + ";";
                emitirTextBasico(gen, pseudo);

                gen.emitInstr(endLabel + ":");
                continue;
            }
        }

        // int a = 10;
        {
            auto m = rxDeclInit.match(line);
            if (m.hasMatch()) {
                std::string id  = m.captured(2).toStdString();
                int value       = m.captured(3).toInt();
                gen.setInitialValue(id, value);
                continue;
            }
        }

        // int d[3] = {...};
        {
            auto m = rxArrayInit.match(line);
            if (m.hasMatch()) {
                std::string id = m.captured(2).toStdString();
                int size       = m.captured(3).toInt();
                QString elems  = m.captured(4).trimmed();

                std::vector<int> values;
                if (!elems.isEmpty()) {
                    QStringList parts = elems.split(
                        QRegularExpression(R"(\s*,\s*)"),
                        Qt::SkipEmptyParts
                        );
                    for (const QString& p : parts) {
                        bool ok = false;
                        int v = p.toInt(&ok);
                        values.push_back(ok ? v : 0);
                    }
                }

                gen.setArraySize(id, size);
                gen.setArrayInitialValues(id, values);
                continue;
            }
        }

        // int d[3];
        {
            auto m = rxArrayDecl.match(line);
            if (m.hasMatch()) {
                std::string id = m.captured(2).toStdString();
                int size       = m.captured(3).toInt();
                if (size > 0)
                    gen.setArraySize(id, size);
                continue;
            }
        }

        // ENTRADA: cin >> ...
        {
            auto m = rxCin.match(line);
            if (m.hasMatch()) {
                QString rest = m.captured(1).trimmed();

                QStringList parts = rest.split(
                    QRegularExpression(R"(\s*>>\s*)"),
                    Qt::SkipEmptyParts
                    );

                for (const QString& exprQ : parts) {
                    QString e = exprQ.trimmed();
                    if (e.isEmpty()) continue;

                    // cin >> d[NUM];
                    auto mc = rxArrIdxConst.match(e);
                    if (mc.hasMatch()) {
                        std::string arr = mc.captured(1).toStdString();
                        int idx         = mc.captured(2).toInt();

                        gen.emitInstr("LDI " + std::to_string(idx));
                        gen.emitInstr("STO $indr");
                        gen.emitInstr("LD $in_port");
                        gen.emitInstr("STOV " + arr);
                        continue;
                    }

                    // cin >> d[i];
                    auto mv = rxArrIdxVar.match(e);
                    if (mv.hasMatch()) {
                        std::string arr = mv.captured(1).toStdString();
                        std::string idx = mv.captured(2).toStdString();
                        idx = mangleIdIfParam(idx);

                        gen.emitLoadId(idx);
                        gen.emitInstr("STO $indr");
                        gen.emitInstr("LD $in_port");
                        gen.emitInstr("STOV " + arr);
                        continue;
                    }

                    // cin >> x;
                    if (rxId.match(e).hasMatch()) {
                        std::string id = e.toStdString();
                        id = mangleIdIfParam(id);
                        gen.emitInstr("LD $in_port");
                        gen.emitStoreId(id);
                        continue;
                    }
                }
                continue;
            }
        }

        // SAÍDA: cout << ...
        {
            auto m = rxCout.match(line);
            if (m.hasMatch()) {
                QString all = m.captured(1).trimmed();

                QStringList parts = all.split(
                    QRegularExpression(R"(\s*<<\s*)"),
                    Qt::SkipEmptyParts
                    );

                for (const QString& exprQ : parts) {
                    QString e = exprQ.trimmed();
                    if (e.isEmpty()) continue;

                    // cout << 55;
                    if (rxLit.match(e).hasMatch()) {
                        gen.emitInstr("LDI " + e.toStdString());
                        gen.emitInstr("STO $out_port");
                        continue;
                    }

                    // cout << d[NUM];
                    auto mc = rxArrIdxConst.match(e);
                    if (mc.hasMatch()) {
                        std::string arr = mc.captured(1).toStdString();
                        int idx         = mc.captured(2).toInt();

                        gen.emitInstr("LDI " + std::to_string(idx));
                        gen.emitInstr("STO $indr");
                        gen.emitInstr("LDV " + arr);
                        gen.emitInstr("STO $out_port");
                        continue;
                    }

                    // cout << d[i];
                    auto mv = rxArrIdxVar.match(e);
                    if (mv.hasMatch()) {
                        std::string arr = mv.captured(1).toStdString();
                        std::string idx = mv.captured(2).toStdString();
                        idx = mangleIdIfParam(idx);

                        gen.emitLoadId(idx);
                        gen.emitInstr("STO $indr");
                        gen.emitInstr("LDV " + arr);
                        gen.emitInstr("STO $out_port");
                        continue;
                    }

                    // cout << x;
                    if (rxId.match(e).hasMatch()) {
                        std::string id = e.toStdString();
                        id = mangleIdIfParam(id);
                        gen.emitLoadId(id);
                        gen.emitInstr("STO $out_port");
                        continue;
                    }
                }
                continue;
            }
        }

        // destino vetor v[algo] = ...
        {
            auto ma = rxAssignArr.match(line);
            if (ma.hasMatch()) {
                std::string arr   = ma.captured(1).toStdString();
                QString idxQ      = ma.captured(2).trimmed();
                QString rhsQ      = ma.captured(3).trimmed();
                std::string rhs   = rhsQ.toStdString();

                bool idxIsLit = rxLit.match(idxQ).hasMatch();
                bool idxIsId  = rxId.match(idxQ).hasMatch();

                // v[NUM] = ...
                if (idxIsLit) {
                    int idx = idxQ.toInt();

                    // v[NUM] = 10; | v[NUM] = x;
                    if (rxLit.match(rhsQ).hasMatch() || rxId.match(rhsQ).hasMatch()) {
                        std::string rhsFixed = rhs;
                        if (rxId.match(rhsQ).hasMatch()) {
                            rhsFixed = mangleIdIfParam(rhsFixed);
                        }
                        gen.emitAssign(arr, true, idx, rhsFixed, false, 0);
                        continue;
                    }

                    // v[NUM] = vet[NUM2];
                    auto rc = rxArrIdxConst.match(rhsQ);
                    if (rc.hasMatch()) {
                        std::string srcArr = rc.captured(1).toStdString();
                        int srcIdx         = rc.captured(2).toInt();
                        gen.emitAssign(arr, true, idx, srcArr, true, srcIdx);
                        continue;
                    }

                    // v[NUM] = vet[j];
                    auto rv = rxArrIdxVar.match(rhsQ);
                    if (rv.hasMatch()) {
                        std::string srcArr = rv.captured(1).toStdString();
                        std::string j      = rv.captured(2).toStdString();
                        j = mangleIdIfParam(j);

                        gen.emitLoadId(j);
                        gen.emitInstr("STO $indr");
                        gen.emitInstr("LDV " + srcArr);
                        gen.emitStoreIdOffset(arr, idx);
                        continue;
                    }

                    // v[NUM] = <expr>
                    if (evalToTmp0(rhsQ)) {
                        gen.emitInstr("LDI " + std::to_string(idx));
                        gen.emitInstr("STO $indr");
                        gen.emitInstr("LD __TMP0");
                        gen.emitInstr("STOV " + arr);
                        continue;
                    }
                }

                // v[i] = ...
                if (idxIsId) {
                    std::string i = idxQ.toStdString();
                    i = mangleIdIfParam(i);

                    // v[i] = 10;
                    if (rxLit.match(rhsQ).hasMatch()) {
                        gen.emitLoadId(i);
                        gen.emitInstr("STO $indr");
                        gen.emitInstr("LDI " + rhs);
                        gen.emitInstr("STOV " + arr);
                        continue;
                    }

                    // v[i] = x;
                    if (rxId.match(rhsQ).hasMatch()) {
                        std::string rhsName = rhs;
                        rhsName = mangleIdIfParam(rhsName);
                        gen.emitAssignVarIndex(arr, i, rhsName);
                        continue;
                    }

                    // v[i] = vet[NUM];
                    auto rc = rxArrIdxConst.match(rhsQ);
                    if (rc.hasMatch()) {
                        std::string srcArr = rc.captured(1).toStdString();
                        int srcIdx         = rc.captured(2).toInt();
                        gen.emitLoadIdOffset(srcArr, srcIdx);
                        gen.emitLoadId(i);
                        gen.emitInstr("STO $indr");
                        gen.emitInstr("STOV " + arr);
                        continue;
                    }

                    // v[i] = vet[j];
                    auto rv = rxArrIdxVar.match(rhsQ);
                    if (rv.hasMatch()) {
                        std::string srcArr = rv.captured(1).toStdString();
                        std::string j      = rv.captured(2).toStdString();
                        j = mangleIdIfParam(j);

                        gen.emitLoadId(j);
                        gen.emitInstr("STO $indr");
                        gen.emitInstr("LDV " + srcArr);

                        gen.emitLoadId(i);
                        gen.emitInstr("STO $indr");
                        gen.emitInstr("STOV " + arr);
                        continue;
                    }

                    // v[i] = <expr>
                    if (evalToTmp0(rhsQ)) {
                        gen.emitLoadId(i);
                        gen.emitInstr("STO $indr");
                        gen.emitInstr("LD __TMP0");
                        gen.emitInstr("STOV " + arr);
                        continue;
                    }
                }

                // v[expr] = ...
                if (!idxIsLit && !idxIsId) {
                    if (!evalToTmp0(idxQ)) {
                        continue;
                    }

                    gen.emitInstr("LD __TMP0");
                    gen.emitInstr("STO $indr");

                    // literal
                    if (rxLit.match(rhsQ).hasMatch()) {
                        gen.emitInstr("LDI " + rhs);
                        gen.emitInstr("STOV " + arr);
                        continue;
                    }

                    // variável
                    if (rxId.match(rhsQ).hasMatch()) {
                        std::string rhsName = rhs;
                        rhsName = mangleIdIfParam(rhsName);
                        gen.emitLoadId(rhsName);
                        gen.emitInstr("STOV " + arr);
                        continue;
                    }

                    // vetor[NUM]
                    auto rc = rxArrIdxConst.match(rhsQ);
                    if (rc.hasMatch()) {
                        std::string srcArr = rc.captured(1).toStdString();
                        int srcIdx         = rc.captured(2).toInt();

                        gen.emitLoadIdOffset(srcArr, srcIdx);
                        gen.emitInstr("STOV " + arr);
                        continue;
                    }

                    // vetor[j]
                    auto rv = rxArrIdxVar.match(rhsQ);
                    if (rv.hasMatch()) {
                        std::string srcArr = rv.captured(1).toStdString();
                        std::string j      = rv.captured(2).toStdString();
                        j = mangleIdIfParam(j);

                        gen.emitLoadId(j);
                        gen.emitInstr("STO $indr");
                        gen.emitInstr("LDV " + srcArr);

                        gen.emitInstr("LD __TMP0");
                        gen.emitInstr("STO $indr");
                        gen.emitInstr("STOV " + arr);
                        continue;
                    }

                    // expressão no RHS
                    if (evalToTmp0(rhsQ)) {
                        gen.emitInstr("LD __TMP0");
                        gen.emitInstr("STOV " + arr);
                        continue;
                    }

                    continue;
                }

                continue;
            }
        }

        // destino escalar x = ...
        {
            auto m = rxAssign.match(line);
            if (m.hasMatch()) {
                std::string dest = m.captured(1).toStdString();
                QString rhsQ     = m.captured(2).trimmed();
                std::string rhs  = rhsQ.toStdString();

                dest = mangleIdIfParam(dest);

                // caso especial: destino recebe retorno de função
                {
                    auto mCallExpr = rxFuncCallExpr.match(rhsQ);
                    if (mCallExpr.hasMatch()) {
                        QString funcNameQ = mCallExpr.captured(1).trimmed();
                        QString argsQ     = mCallExpr.captured(2).trimmed();

                        QStringList argExprs;
                        if (!argsQ.isEmpty()) {
                            argExprs = argsQ.split(
                                QRegularExpression(R"(\s*,\s*)"),
                                Qt::SkipEmptyParts
                                );
                        }

                        gerarChamadaFuncao(funcNameQ, argExprs);
                        gen.emitStoreId(dest);
                        continue;
                    }
                }

                // tenta N-ária
                if (emitAssignNaryToDest(dest, rhsQ)) {
                    continue;
                }

                // fallback binário
                auto mb = rxBin.match(rhsQ);
                if (mb.hasMatch()) {
                    std::string op1  = mb.captured(1).toStdString();
                    std::string oper = mb.captured(2).toStdString();
                    std::string op2  = mb.captured(3).toStdString();

                    QString op1Q = QString::fromStdString(op1);
                    QString op2Q = QString::fromStdString(op2);

                    if (rxId.match(op1Q).hasMatch()) {
                        op1 = mangleIdIfParam(op1);
                    }
                    if (rxId.match(op2Q).hasMatch()) {
                        op2 = mangleIdIfParam(op2);
                    }

                    gen.emitAssignSimpleExpr(dest, op1, oper, op2);
                    continue;
                }

                // x = 10;
                if (rxLit.match(rhsQ).hasMatch()) {
                    gen.emitAssignSimpleExpr(dest, rhs, "", "");
                    continue;
                }

                // x = y;
                if (rxId.match(rhsQ).hasMatch()) {
                    std::string rhsId = rhsQ.toStdString();
                    rhsId = mangleIdIfParam(rhsId);
                    gen.emitAssign(dest, false, 0, rhsId, false, 0);
                    continue;
                }

                // x = vet[NUM];
                auto rc = rxArrIdxConst.match(rhsQ);
                if (rc.hasMatch()) {
                    std::string arr = rc.captured(1).toStdString();
                    int idx         = rc.captured(2).toInt();
                    gen.emitLoadIdOffset(arr, idx);
                    gen.emitStoreId(dest);
                    continue;
                }

                // x = vet[i];
                auto rv = rxArrIdxVar.match(rhsQ);
                if (rv.hasMatch()) {
                    std::string arr = rv.captured(1).toStdString();
                    std::string i   = rv.captured(2).toStdString();
                    i = mangleIdIfParam(i);

                    gen.emitLoadId(i);
                    gen.emitInstr("STO $indr");
                    gen.emitInstr("LDV " + arr);
                    gen.emitStoreId(dest);
                    continue;
                }

                continue;
            }
        }
        // resto ignorado
    }
}

// monta o texto do assembly completo (.data + .text)
// e também salva em "programa.asm"
static void exibirProgramaASM(const std::string& program,
                              QPlainTextEdit* destinoAsmView,
                              std::function<void(const QString&)> logFn)
{
    const QString asmText = QString::fromStdString(program);

    QFile f("programa.asm");
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(asmText.toUtf8());
        f.close();
        if (logFn) logFn("Gerado arquivo: programa.asm");
    } else {
        if (logFn) logFn("Aviso: não foi possível salvar o arquivo programa.asm");
    }

    if (destinoAsmView) {
        destinoAsmView->clear();
        destinoAsmView->setPlainText(asmText);
    }
}

// MainWindow
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Conecta o botão "Compilar" ao slot
    connect(ui->Compilar, &QPushButton::clicked, this, &MainWindow::tratarCliqueBotao);

    // --- Tabela de Símbolos (QTableView) ---
    modelSimbolos = new QStandardItemModel(this);
    modelSimbolos->setColumnCount(6);
    modelSimbolos->setHorizontalHeaderLabels(
        {"Nome", "Tipo", "Modalidade", "Escopo", "Usado", "Inicializado"}
        );

    ui->tableView->setModel(modelSimbolos);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Se o .ui NÃO tiver um QPlainTextEdit chamado "Asm", criamos um Dock "ASM"
    if (!this->findChild<QPlainTextEdit*>("Asm")) {
        auto *dockAsm = new QDockWidget(tr("ASM"), this);
        dockAsm->setObjectName("dockAsm");
        dockAsm->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

        auto *asmView = new QPlainTextEdit(dockAsm);
        asmView->setObjectName("asmView");
        asmView->setReadOnly(true);
        dockAsm->setWidget(asmView);

        addDockWidget(Qt::RightDockWidgetArea, dockAsm);
    }
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::tratarCliqueBotao()
{
    // Limpa a saída anterior
    ui->Console->clear();
    modelSimbolos->removeRows(0, modelSimbolos->rowCount());

    // LIMPAR ASM LOGO NO COMEÇO
    QPlainTextEdit* asmUi = this->findChild<QPlainTextEdit*>("Asm");
    if (!asmUi) {
        if (auto *dock = this->findChild<QDockWidget*>("dockAsm")) {
            asmUi = dock->findChild<QPlainTextEdit*>("asmView");
        }
    }
    if (asmUi) {
        asmUi->clear();
    }

    const QString fonte = ui->Entrada->toPlainText();
    if (fonte.trimmed().isEmpty()) {
        ui->Console->appendPlainText("Nada para compilar.");
        return;
    }

    // Instancia o pipeline GALS
    Lexico    lex;
    Sintatico sint;
    Semantico sem;

    // Gera opções do gerador de código
    CodeGeneratorBIP::Options opt;
    opt.includeDataHeader = true;
    opt.includeTextHeader = true;
    opt.entryLabel        = "_PRINCIPAL";

    CodeGeneratorBIP gen(opt);

    // conecta o semântico ao gerador
    sem.setCodeGenerator(&gen);

    // alimenta o léxico com o código-fonte
    const QByteArray fonteUtf8 = fonte.toUtf8();
    lex.setInput(fonteUtf8.constData());

    // manda mensagens do semântico para o Console
    sem.clearMensagens();
    sem.setLogger([this](const std::string& msg) {
        ui->Console->appendPlainText(QString::fromStdString(msg));
    });

    // 1) Fase de análise (léxica/sintática/semântica)
    try {
        sint.parse(&lex, &sem);
    }
    catch (const LexicalError &err) {
        ui->Console->appendPlainText(
            QString("Erro Léxico: %1 - posição: %2")
                .arg(toQString(err.getMessage()))
                .arg(err.getPosition()));
        return; // NÃO segue para geração de ASM
    }
    catch (const SyntacticError &err) {
        ui->Console->appendPlainText(
            QString("Erro Sintático: %1 - posição: %2")
                .arg(toQString(err.getMessage()))
                .arg(err.getPosition()));
        return; // NÃO segue para geração de ASM
    }
    catch (const SemanticError &err) {
        ui->Console->appendPlainText(
            QString("Erro Semântico: %1 - posição: %2")
                .arg(toQString(err.getMessage()))
                .arg(err.getPosition()));
        return; // NÃO segue para geração de ASM
    }

    // 2) Se o semântico marcou erros "fatais", não gera ASM
    if (sem.temErro()) {
        ui->Console->appendPlainText(
            "Foram encontrados erros semânticos. Assembly não será gerado.");
        return;
    }

    // 3) Garante que a execução comece em MAIN (main() gerado como rótulo MAIN)
    gen.emitInstr("JMP MAIN");

    // 4) Geração do .text
    emitirTextBasico(gen, fonte);

    // Marca 'main' como usada (ponto de entrada)
    for (auto& s : sem.tabelaSimbolo) {
        if (s.nome == "main" && s.modalidade == "funcao") {
            s.usado = true;
            break;
        }
    }

    sem.verificarNaoUsados();

    ui->Console->appendPlainText("Compilado com sucesso!");
    ui->Console->appendPlainText("Símbolos declarados:");

    for (size_t i = 0; i < sem.tabelaSimbolo.size(); ++i) {
        const Simbolo& s = sem.tabelaSimbolo.at(i);
        std::ostringstream oss;
        oss << s;
        ui->Console->appendPlainText(QString::fromStdString(oss.str()));
    }

    // Cria cópia da tabela para incluir parâmetros "manglados"
    std::vector<Simbolo> tabelaFinal = sem.tabelaSimbolo;

    // Para cada símbolo "parâmetro" usado, cria um símbolo novo COISA_param
    for (const auto &s : sem.tabelaSimbolo) {
        if (s.modalidade == "parametro" && s.usado) {
            Simbolo novo = s;

            std::string funcName  = s.escopo;  // escopo = nome da função
            std::string paramName = s.nome;

            novo.nome       = funcName + "_" + paramName;
            novo.escopo     = "global";
            novo.modalidade = "variavel";

            tabelaFinal.push_back(novo);
        }
    }

    // preenche a tabela de símbolos exibida na UI
    preencherTabelaSimbolos(tabelaFinal);

    std::string program = gen.buildProgram(tabelaFinal);

    exibirProgramaASM(program, asmUi,
                      [this](const QString& m){ ui->Console->appendPlainText(m); });

    qDebug() << "Compilado com sucesso";
}
