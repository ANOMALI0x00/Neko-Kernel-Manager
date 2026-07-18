#pragma once

#include <QObject>
#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QFileSystemWatcher>

// Tracks the system theme the same way Neko Wizard does: it reads the standard
// libadwaita colour tokens (window_bg_color, accent_bg_color, ...) that Noctalia's
// template processor — or matugen / pywal / stock libadwaita — writes into the
// user's GTK config. When that file changes the theme reloads live. When no such
// file is present it falls back to the built-in "minimal / flat / sharp" dark
// palette, so the app looks right on any system.
class ThemeManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QColor windowBg    READ windowBg    NOTIFY changed)
    Q_PROPERTY(QColor windowFg    READ windowFg    NOTIFY changed)
    Q_PROPERTY(QColor viewBg      READ viewBg      NOTIFY changed)
    Q_PROPERTY(QColor cardBg      READ cardBg      NOTIFY changed)
    Q_PROPERTY(QColor headerbarBg READ headerbarBg NOTIFY changed)
    Q_PROPERTY(QColor headerbarFg READ headerbarFg NOTIFY changed)
    Q_PROPERTY(QColor popoverBg   READ popoverBg   NOTIFY changed)
    Q_PROPERTY(QColor accentBg    READ accentBg    NOTIFY changed)
    Q_PROPERTY(QColor accentFg    READ accentFg    NOTIFY changed)
    Q_PROPERTY(QColor errorBg     READ errorBg     NOTIFY changed)
    Q_PROPERTY(QColor errorFg     READ errorFg     NOTIFY changed)
    Q_PROPERTY(bool    themed     READ themed      NOTIFY changed)
    Q_PROPERTY(QString source     READ source      NOTIFY changed)

public:
    explicit ThemeManager(QObject *parent = nullptr);

    QColor windowBg()    const { return get("window_bg_color",    QColor("#101010")); }
    QColor windowFg()    const { return get("window_fg_color",    QColor("#ececec")); }
    QColor viewBg()      const { return get("view_bg_color",      windowBg()); }
    QColor cardBg()      const { return get("card_bg_color",      QColor("#171717")); }
    QColor headerbarBg() const { return get("headerbar_bg_color", windowBg()); }
    QColor headerbarFg() const { return get("headerbar_fg_color", windowFg()); }
    QColor popoverBg()   const { return get("popover_bg_color",   cardBg()); }
    QColor accentBg()    const { return get("accent_bg_color",    QColor("#4a90e2")); }
    QColor accentFg()    const { return get("accent_fg_color",    QColor("#ffffff")); }
    QColor errorBg()     const { return get("error_bg_color",     QColor("#cc3b33")); }
    QColor errorFg()     const { return get("error_fg_color",     QColor("#ffffff")); }
    bool    themed()     const { return m_themed; }
    QString source()     const { return m_source; }

    // Force a re-read (also wired to the file watcher)
    Q_INVOKABLE void reload();

signals:
    void changed();

private:
    QColor get(const QString &name, const QColor &fallback) const;
    void load();
    void onWatch();

    QHash<QString, QColor> m_colors;
    bool m_themed = false;
    QString m_source;
    QFileSystemWatcher m_watcher;
    QStringList m_files;
    QString m_dir;
};
