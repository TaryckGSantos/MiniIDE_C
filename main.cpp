#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "MiniIDE_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;
    w.show();
    return a.exec();
}

/*#include "Sintatico.h"
#include "Semantico.h"
#include <iostream>

using namespace std;

int main() {
    Lexico    lex;
    Sintatico sint;
    Semantico sem;

    lex.setInput("int a,b; float c,d;");

    try {
        sint.parse(&lex, &sem);
        cout << "Compilado com sucesso!" << endl;
        cout << "Símbolos declarados:" << endl;

        for (size_t i = 0; i < sem.tabelaSimbolo.size(); ++i) {
            const Simbolo& s = sem.tabelaSimbolo.at(i);
            cout << s;
        }
    }
    catch (LexicalError& err) {
        cerr << "Erro Léxico: " << err.getMessage()
             << " - na posição: " << err.getPosition() << endl;
    }
    catch (SyntacticError& err) {
        cerr << "Erro Sintático: " << err.getMessage()
             << " - na posição: " << err.getPosition() << endl;
    }
    catch (SemanticError& err) {
        cerr << "Erro Semântico: " << err.getMessage()
             << " - na posição: " << err.getPosition() << endl;
    }
    return 0;
}
*/
