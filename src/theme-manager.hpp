#pragma once

#include <QObject>
#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>

// Matecito Dark (GTK3) Theme Manager.
// Forces the consistent Matecito Dark appearance across all environments,
// preventing external system palettes or desktop themes from altering the UI.
class ThemeManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QColor windowBg    READ windowBg    CONSTANT)
    Q_PROPERTY(QColor windowFg    READ windowFg    CONSTANT)
    Q_PROPERTY(QColor viewBg      READ viewBg      CONSTANT)
    Q_PROPERTY(QColor cardBg      READ cardBg      CONSTANT)
    Q_PROPERTY(QColor headerbarBg READ headerbarBg CONSTANT)
    Q_PROPERTY(QColor headerbarFg READ headerbarFg CONSTANT)
    Q_PROPERTY(QColor popoverBg   READ popoverBg   CONSTANT)
    Q_PROPERTY(QColor accentBg    READ accentBg    CONSTANT)
    Q_PROPERTY(QColor accentFg    READ accentFg    CONSTANT)
    Q_PROPERTY(QColor errorBg     READ errorBg     CONSTANT)
    Q_PROPERTY(QColor errorFg     READ errorFg     CONSTANT)
    Q_PROPERTY(QColor sidebarBg   READ sidebarBg   CONSTANT)
    Q_PROPERTY(QColor sidebarFg   READ sidebarFg   CONSTANT)
    Q_PROPERTY(bool    themed     READ themed      CONSTANT)
    Q_PROPERTY(QString source     READ source      CONSTANT)

public:
    explicit ThemeManager(QObject *parent = nullptr);

    // Matecito Dark (GTK3) forced palette:
    // accent #8da383 (sage green), window/view #1e2320, card/headerbar #272e2a, text #d3cdc3
    QColor windowBg()    const { return get("window_bg_color",    QColor("#1e2320")); }
    QColor windowFg()    const { return get("window_fg_color",    QColor("#d3cdc3")); }
    QColor viewBg()      const { return get("view_bg_color",      windowBg()); }
    QColor cardBg()      const { return get("card_bg_color",      QColor("#272e2a")); }
    QColor headerbarBg() const { return get("headerbar_bg_color", QColor("#272e2a")); }
    QColor headerbarFg() const { return get("headerbar_fg_color", windowFg()); }
    QColor popoverBg()   const { return get("popover_bg_color",   cardBg()); }
    QColor accentBg()    const { return get("accent_bg_color",    QColor("#8da383")); }
    QColor accentFg()    const { return get("accent_fg_color",    QColor("#1e2320")); }
    QColor errorBg()     const { return get("error_bg_color",     QColor("#c27979")); }
    QColor errorFg()     const { return get("error_fg_color",     QColor("#1e2320")); }
    QColor sidebarBg()   const { return get("sidebar_bg_color",   cardBg()); }
    QColor sidebarFg()   const { return get("sidebar_fg_color",   windowFg()); }
    bool    themed()     const { return m_themed; }
    QString source()     const { return m_source; }

    Q_INVOKABLE void reload();

signals:
    void changed();

private:
    QColor get(const QString &name, const QColor &fallback) const;
    void load();

    QHash<QString, QColor> m_colors;
    bool m_themed = true;
    QString m_source;
    QStringList m_files;
};
