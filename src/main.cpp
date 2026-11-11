#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>
#include <QQuickWindow>
#include <QTimer>
#include <cstdlib>
#include <memory>
#include "kernel-bridge.hpp"
#include "theme-manager.hpp"

int main(int argc, char *argv[]) {
    // Forzar estilo Fusion y evitar que plugins de escritorio (qt6ct, Kvantum, KDE, GNOME)
    // o variables de entorno alteren la paleta y apariencia de la aplicación
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
    qputenv("QT_STYLE_OVERRIDE", "Fusion");
    qunsetenv("QT_QPA_PLATFORMTHEME");

    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // El nombre de la aplicación debe coincidir con el ID del portal (neko-kernel-manager)
    QCoreApplication::setApplicationName("neko-kernel-manager");
    QCoreApplication::setApplicationVersion("1.4.0");
    QCoreApplication::setOrganizationName("Neko");
    QCoreApplication::setOrganizationDomain("com.neko");
    QGuiApplication::setDesktopFileName("neko-kernel-manager");

    QApplication app(argc, argv);
    app.setApplicationDisplayName("Neko Kernel Manager");
    app.setWindowIcon(QIcon(":/neko/Data/logo.png"));

    // Parse command line arguments
    bool verbose = false;
    QString screenshotPath;
    QString screenshotDialog;
    int screenshotTab = 0;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--verbose" || arg == "-v" || arg == "--debug") {
            verbose = true;
        } else if (arg == "--screenshot" && i + 1 < argc) {
            screenshotPath = QString::fromLocal8Bit(argv[++i]);
        } else if (arg == "--screenshot-tab" && i + 1 < argc) {
            screenshotTab = QString::fromLocal8Bit(argv[++i]).toInt();
        } else if (arg == "--screenshot-dialog" && i + 1 < argc) {
            screenshotDialog = QString::fromLocal8Bit(argv[++i]);
        }
    }

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

    // Aplicar paleta activa de ThemeManager a todos los componentes nativos y estados
    auto applyNative = [&app, &theme]() {
        const QColor bg = theme.windowBg(), fg = theme.windowFg(), view = theme.viewBg(),
                     card = theme.cardBg(), accent = theme.accentBg(), onAccent = theme.accentFg(),
                     disabledFg = theme.faintFg(), disabledBg = theme.disabledBg(),
                     btnBg = theme.buttonBg(), btnHover = theme.buttonHoverBg();
        QColor borderCol = fg;
        borderCol.setAlphaF(0.18);

        QPalette p;
        p.setColor(QPalette::Window, bg);
        p.setColor(QPalette::WindowText, fg);
        p.setColor(QPalette::Base, view);
        p.setColor(QPalette::AlternateBase, card);
        p.setColor(QPalette::ToolTipBase, card);
        p.setColor(QPalette::ToolTipText, fg);
        p.setColor(QPalette::Text, fg);
        p.setColor(QPalette::Button, btnBg);
        p.setColor(QPalette::ButtonText, fg);
        p.setColor(QPalette::BrightText, fg);
        p.setColor(QPalette::Link, accent);
        p.setColor(QPalette::Highlight, accent);
        p.setColor(QPalette::HighlightedText, onAccent);
        p.setColor(QPalette::PlaceholderText, disabledFg);

        // Roles 3D/bisel de Fusion derivados del tema activo
        p.setColor(QPalette::Light, btnHover);
        p.setColor(QPalette::Midlight, btnBg);
        p.setColor(QPalette::Mid, card);
        p.setColor(QPalette::Dark, disabledBg);
        p.setColor(QPalette::Shadow, theme.shadowColor());

        // Estados deshabilitados coherentes con el tema
        p.setColor(QPalette::Disabled, QPalette::WindowText, disabledFg);
        p.setColor(QPalette::Disabled, QPalette::Text, disabledFg);
        p.setColor(QPalette::Disabled, QPalette::ButtonText, disabledFg);
        p.setColor(QPalette::Disabled, QPalette::Button, disabledBg);
        p.setColor(QPalette::Disabled, QPalette::Base, disabledBg);
        p.setColor(QPalette::Disabled, QPalette::Highlight, theme.disabledHighlight());

        app.setPalette(p);

        app.setStyleSheet(QString(
            "QWidget { background-color:%1; color:%2; font-family:'0xProto Nerd Font'; }"
            "QMenu { background-color:%3; color:%2; border:1px solid %6; padding:4px; }"
            "QMenu::item:selected { background-color:%4; color:%5; border-radius:4px; }"
            "QComboBox { background-color:%1; color:%2; border:1px solid %3; padding:2px 10px; }"
            "QComboBox QAbstractItemView { background-color:%1; color:%2;"
            " selection-background-color:%4; selection-color:%5; border:1px solid %4; outline:none; }"
            "QComboBox QAbstractItemView::item { color:%2; padding:8px; background-color:transparent; min-height:30px; }"
            "QComboBox QAbstractItemView::item:selected { background-color:%4; color:%5; }"
            "QToolTip { background-color:%3; color:%2; border:1px solid %4; }"
        ).arg(bg.name(), fg.name(), card.name(), accent.name(), onAccent.name(),
              QString("rgba(%1,%2,%3,0.18)").arg(fg.red()).arg(fg.green()).arg(fg.blue())));
    };
    applyNative();

    qmlRegisterType<KernelBridge>("Neko", 1, 0, "KernelBridge");

    KernelBridge bridge;
    auto syncLogPalette = [&bridge, &theme]() {
        bridge.setLogPalette(
            theme.logErrorColor().name(),
            theme.logSuccessColor().name(),
            theme.logWarningColor().name(),
            theme.logInfoColor().name()
        );
    };
    syncLogPalette();
    QObject::connect(&theme, &ThemeManager::changed, &app, [applyNative, syncLogPalette]() {
        applyNative();
        syncLogPalette();
    });
    // Activar modo verbose si se pasó el argumento
    if (verbose) {
        bridge.setVerbose(true);
        bridge.appendLog("Verbose mode enabled.");
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("bridge", &bridge);
    engine.load(QUrl(QStringLiteral("qrc:/neko/src/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    if (!screenshotPath.isEmpty()) {
        QQuickWindow *win = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (win) {
            auto takeShot = [&app, win, screenshotPath, screenshotTab, screenshotDialog]() {
                if (!screenshotDialog.isEmpty()) {
                    QObject *dlg = win->findChild<QObject*>(screenshotDialog);
                    if (dlg) {
                        QMetaObject::invokeMethod(dlg, "open");
                    }
                } else if (screenshotTab > 0) {
                    QObject *tabs = win->findChild<QObject*>("detailTabs");
                    if (tabs) {
                        tabs->setProperty("currentIndex", screenshotTab);
                    }
                }
                QTimer::singleShot(1500, &app, [win, screenshotPath, &app]() {
                    QImage img = win->grabWindow();
                    img.save(screenshotPath);
                    qInfo() << "Screenshot saved to:" << screenshotPath;
                    app.quit();
                });
            };

            auto conn = std::make_shared<QMetaObject::Connection>();
            *conn = QObject::connect(&bridge, &KernelBridge::busyChanged, &app, [&bridge, takeShot, conn]() {
                if (!bridge.busy()) {
                    QObject::disconnect(*conn);
                    // Esperar a que la lista de kernels se pueble y la UI seleccione el kernel activo
                    QTimer::singleShot(1500, takeShot);
                }
            });
        }
    } else {
        QTimer::singleShot(250, &bridge, &KernelBridge::startPrivilegedSession);
    }

    return app.exec();
}