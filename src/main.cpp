#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>
#include "kernel-bridge.hpp"

int main(int argc, char *argv[]) {
    // Set style to Fusion BEFORE creating QApplication to ensure consistency
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/neko/Data/logo.png"));

    // Load Nerd Fonts
    QStringList fonts = {
        ":/neko/Data/fonts/0xProtoNerdFont-Regular.ttf",
        ":/neko/Data/fonts/0xProtoNerdFont-Bold.ttf",
        ":/neko/Data/fonts/0xProtoNerdFont-Italic.ttf",
        ":/neko/Data/fonts/0xProtoNerdFontMono-Regular.ttf",
        ":/neko/Data/fonts/0xProtoNerdFontMono-Bold.ttf",
        ":/neko/Data/fonts/0xProtoNerdFontMono-Italic.ttf",
        ":/neko/Data/fonts/0xProtoNerdFontPropo-Regular.ttf",
        ":/neko/Data/fonts/0xProtoNerdFontPropo-Bold.ttf",
        ":/neko/Data/fonts/0xProtoNerdFontPropo-Italic.ttf"
    };

    for (const QString &fontPath : fonts) {
        if (QFontDatabase::addApplicationFont(fontPath) == -1) {
            qWarning() << "Failed to load font:" << fontPath;
        }
    }

    // Set default font
    app.setFont(QFont("0xProto Nerd Font", 10));

    QTranslator translator;
    QString translationsPath = QCoreApplication::applicationDirPath() + "/translations";
    QLocale systemLocale = QLocale::system();
    bool translationLoaded = false;

    translationLoaded = translator.load(systemLocale, "app", "_", translationsPath);
    if (!translationLoaded) {
        translationLoaded = translator.load(systemLocale, "app", "_", ":/i18n");
    }
    if (!translationLoaded) {
        QString shortLocale = systemLocale.name().section('_', 0, 0);
        translationLoaded = translator.load(QString("app_%1").arg(shortLocale), translationsPath);
    }
    if (!translationLoaded) {
        QString shortLocale = systemLocale.name().section('_', 0, 0);
        translationLoaded = translator.load(QString("app_%1").arg(shortLocale), ":/i18n");
    }
    if (translationLoaded) {
        app.installTranslator(&translator);
    }

    // Force a dark palette to prevent black text issues (User solution part 3-ish)
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor("#1e1e2e"));
    darkPalette.setColor(QPalette::WindowText, QColor("#cdd6f4"));
    darkPalette.setColor(QPalette::Base, QColor("#1e1e2e"));
    darkPalette.setColor(QPalette::AlternateBase, QColor("#25263a"));
    darkPalette.setColor(QPalette::ToolTipBase, QColor("#cdd6f4"));
    darkPalette.setColor(QPalette::ToolTipText, QColor("#cdd6f4"));
    darkPalette.setColor(QPalette::Text, QColor("#cdd6f4"));
    darkPalette.setColor(QPalette::Button, QColor("#1e1e2e"));
    darkPalette.setColor(QPalette::ButtonText, QColor("#cdd6f4"));
    darkPalette.setColor(QPalette::BrightText, QColor(Qt::white));
    darkPalette.setColor(QPalette::Link, QColor("#cba6f7"));
    darkPalette.setColor(QPalette::Highlight, QColor("#a6e3a1"));
    darkPalette.setColor(QPalette::HighlightedText, QColor("#1e1e2e"));
    app.setPalette(darkPalette);

    // Apply global stylesheet (User solution part 1)
    app.setStyleSheet(
        "QComboBox {"
        "  background-color: #1e1e2e;"
        "  color: #cdd6f4;"
        "  border: 1px solid #313244;"
        "  border-radius: 8px;"
        "  padding: 2px 10px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: #1e1e2e;"
        "  color: #cdd6f4;"
        "  selection-background-color: #45475a;"
        "  selection-color: #a6e3a1;"
        "  border: 1px solid #a6e3a1;"
        "  outline: none;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "  color: #cdd6f4;"
        "  padding: 8px;"
        "  background-color: transparent;"
        "  min-height: 30px;"
        "}"
        "QComboBox QAbstractItemView::item:selected {"
        "  background-color: #45475a;"
        "  color: #a6e3a1;"
        "}"
        "QListView {"
        "  background-color: #1e1e2e;"
        "  color: #cdd6f4;"
        "}"
        "QListView::item {"
        "  color: #cdd6f4;"
        "}"
        "QListView::item:selected {"
        "  background-color: #45475a;"
        "  color: #a6e3a1;"
        "}"
    );

    qmlRegisterType<KernelBridge>("Neko", 1, 0, "KernelBridge");

    KernelBridge bridge;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("bridge", &bridge);
    engine.load(QUrl(QStringLiteral("qrc:/neko/src/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}