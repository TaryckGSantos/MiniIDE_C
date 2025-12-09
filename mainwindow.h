#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#pragma once

#include <QMainWindow>
#include <QStandardItemModel>

#include "Lexico.h"
#include "Sintatico.h"
#include "Semantico.h"
#include "LexicalError.h"
#include "SyntacticError.h"
#include "SemanticError.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void tratarCliqueBotao();   // slot do botão Compilar

private:
    Ui::MainWindow *ui;

    // Modelo da Tabela de Símbolos (renderizado no ui->tableView)
    QStandardItemModel *modelSimbolos = nullptr;

    // Helper para preencher o QTableView com os símbolos do semântico
    void preencherTabelaSimbolos(const std::vector<Simbolo>& tabela);

    // Converte mensagens/strings para QString
    static QString toQString(const QString &s) { return s; }
    static QString toQString(const std::string &s) { return QString::fromStdString(s); }
    static QString toQString(const char *s) { return QString::fromUtf8(s ? s : ""); }
};

#endif // MAINWINDOW_H
