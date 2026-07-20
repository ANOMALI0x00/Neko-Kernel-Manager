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
#include "theme-manager.hpp"

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

    // System theme (Noctalia / matugen / pywal / libadwaita), with a dark fallback.
    ThemeManager theme;

    // Keep native widgets (tooltips, native popups) in step with the theme, live.
    auto applyNative = [&app, &theme]() {
        const QColor bg = theme.windowBg(), fg = theme.windowFg(), view = theme.viewBg(),
                     card = theme.cardBg(), accent = theme.accentBg(), onAccent = theme.accentFg();
        QPalette p;
        p.setColor(QPalette::Window, bg);
        p.setColor(QPalette::WindowText, fg);
        p.setColor(QPalette::Base, view);
        p.setColor(QPalette::AlternateBase, card);
        p.setColor(QPalette::ToolTipBase, card);
        p.setColor(QPalette::ToolTipText, fg);
        p.setColor(QPalette::Text, fg);
        p.setColor(QPalette::Button, card);
        p.setColor(QPalette::ButtonText, fg);
        p.setColor(QPalette::BrightText, QColor(Qt::white));
        p.setColor(QPalette::Link, accent);
        p.setColor(QPalette::Highlight, accent);
        p.setColor(QPalette::HighlightedText, onAccent);
        app.setPalette(p);

        app.setStyleSheet(QString(
            "QComboBox { background-color:%1; color:%2; border:1px solid %3; padding:2px 10px; }"
            "QComboBox QAbstractItemView { background-color:%1; color:%2;"
            " selection-background-color:%4; selection-color:%5; border:1px solid %4; outline:none; }"
            "QComboBox QAbstractItemView::item { color:%2; padding:8px; background-color:transparent; min-height:30px; }"
            "QComboBox QAbstractItemView::item:selected { background-color:%4; color:%5; }"
            "QToolTip { background-color:%3; color:%2; border:1px solid %4; }"
        ).arg(bg.name(), fg.name(), card.name(), accent.name(), onAccent.name()));
    };
    applyNative();
    QObject::connect(&theme, &ThemeManager::changed, &app, applyNative);

    qmlRegisterType<KernelBridge>("Neko", 1, 0, "KernelBridge");

    KernelBridge bridge;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("bridge", &bridge);
    engine.load(QUrl(QStringLiteral("qrc:/neko/src/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}