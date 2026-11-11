#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

// Modular GTK3-compatible Theme Manager.
// Reads @define-color tokens from user override (~/.config/neko-kernel-manager/theme.css),
// local/installed theme files, or the bundled Matecito Dark stylesheet, and computes
// derived UI tokens so changing themes does not require touching application code.
class ThemeManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QColor windowBg           READ windowBg           NOTIFY changed)
    Q_PROPERTY(QColor windowFg           READ windowFg           NOTIFY changed)
    Q_PROPERTY(QColor viewBg             READ viewBg             NOTIFY changed)
    Q_PROPERTY(QColor cardBg             READ cardBg             NOTIFY changed)
    Q_PROPERTY(QColor headerbarBg        READ headerbarBg        NOTIFY changed)
    Q_PROPERTY(QColor headerbarFg        READ headerbarFg        NOTIFY changed)
    Q_PROPERTY(QColor popoverBg          READ popoverBg          NOTIFY changed)
    Q_PROPERTY(QColor accentBg           READ accentBg           NOTIFY changed)
    Q_PROPERTY(QColor accentFg           READ accentFg           NOTIFY changed)
    Q_PROPERTY(QColor errorBg            READ errorBg            NOTIFY changed)
    Q_PROPERTY(QColor errorFg            READ errorFg            NOTIFY changed)
    Q_PROPERTY(QColor warningColor       READ warningColor       NOTIFY changed)
    Q_PROPERTY(QColor successColor       READ successColor       NOTIFY changed)
    Q_PROPERTY(QColor infoColor          READ infoColor          NOTIFY changed)
    Q_PROPERTY(QColor mutedFg            READ mutedFg            NOTIFY changed)
    Q_PROPERTY(QColor faintFg            READ faintFg            NOTIFY changed)
    Q_PROPERTY(QColor buttonBg           READ buttonBg           NOTIFY changed)
    Q_PROPERTY(QColor buttonHoverBg      READ buttonHoverBg      NOTIFY changed)
    Q_PROPERTY(QColor buttonActiveBg     READ buttonActiveBg     NOTIFY changed)
    Q_PROPERTY(QColor inputBg            READ inputBg            NOTIFY changed)
    Q_PROPERTY(QColor disabledBg         READ disabledBg         NOTIFY changed)
    Q_PROPERTY(QColor disabledHighlight  READ disabledHighlight  NOTIFY changed)
    Q_PROPERTY(QColor shadowColor        READ shadowColor        NOTIFY changed)
    Q_PROPERTY(QColor sidebarBg          READ sidebarBg          NOTIFY changed)
    Q_PROPERTY(QColor sidebarFg          READ sidebarFg          NOTIFY changed)
    Q_PROPERTY(QColor logErrorColor      READ logErrorColor      NOTIFY changed)
    Q_PROPERTY(QColor logSuccessColor    READ logSuccessColor    NOTIFY changed)
    Q_PROPERTY(QColor logWarningColor    READ logWarningColor    NOTIFY changed)
    Q_PROPERTY(QColor logInfoColor       READ logInfoColor       NOTIFY changed)
    Q_PROPERTY(bool    themed            READ themed             NOTIFY changed)
    Q_PROPERTY(QString source            READ source             NOTIFY changed)

public:
    explicit ThemeManager(QObject *parent = nullptr);

    QColor windowBg()          const { return get("window_bg_color",    QColor("#1e2320")); }
    QColor windowFg()          const { return get("window_fg_color",    QColor("#d3cdc3")); }
    QColor viewBg()            const { return get("view_bg_color",      windowBg()); }
    QColor cardBg()            const { return get("card_bg_color",      QColor("#272e2a")); }
    QColor headerbarBg()       const { return get("headerbar_bg_color", cardBg()); }
    QColor headerbarFg()       const { return get("headerbar_fg_color", windowFg()); }
    QColor popoverBg()         const { return get("popover_bg_color",   cardBg()); }
    QColor accentBg()          const { return get("accent_bg_color",    QColor("#8da383")); }
    QColor accentFg()          const { return get("accent_fg_color",    windowBg()); }
    QColor errorBg()           const { return get("error_bg_color",     QColor("#c27979")); }
    QColor errorFg()           const { return get("error_fg_color",     windowBg()); }
    QColor warningColor()      const { return get("warning_color",      QColor("#d49a5b")); }
    QColor successColor()      const { return get("success_color",      accentBg()); }
    QColor infoColor()         const { return get("info_color",         mix(accentBg(), windowFg(), 0.25)); }
    QColor mutedFg()           const { return get("muted_fg_color",     mix(windowFg(), windowBg(), 0.45)); }
    QColor faintFg()           const { return get("faint_fg_color",     mix(windowFg(), windowBg(), 0.68)); }
    QColor buttonBg()          const { return get("button_bg_color",    mix(cardBg(), windowFg(), 0.04)); }
    QColor buttonHoverBg()     const { return get("button_hover_color", mix(cardBg(), windowFg(), 0.11)); }
    QColor buttonActiveBg()    const { return get("button_active_color", mix(cardBg(), windowBg(), 0.35)); }
    QColor inputBg()           const { return get("input_bg_color",     mix(cardBg(), windowBg(), 0.58)); }
    QColor disabledBg()        const { return inputBg(); }
    QColor disabledHighlight() const { return buttonHoverBg(); }
    QColor shadowColor()       const { return mix(windowBg(), QColor("#000000"), 0.45); }
    QColor sidebarBg()         const { return get("sidebar_bg_color",   cardBg()); }
    QColor sidebarFg()         const { return get("sidebar_fg_color",   windowFg()); }
    QColor logErrorColor()     const { return get("log_error_color",    mix(errorBg(), QColor("#ff6b6b"), 0.55)); }
    QColor logSuccessColor()   const { return get("log_success_color",  mix(accentBg(), QColor("#6ee79a"), 0.45)); }
    QColor logWarningColor()   const { return get("log_warning_color",  mix(warningColor(), QColor("#f5e393"), 0.5)); }
    QColor logInfoColor()      const { return get("log_info_color",     mix(infoColor(), windowFg(), 0.25)); }
    bool    themed()           const { return m_themed; }
    QString source()           const { return m_source; }

    Q_INVOKABLE void reload();

signals:
    void changed();

private:
    static QColor mix(const QColor &a, const QColor &b, qreal amount);
    QColor get(const QString &name, const QColor &fallback) const;
    void load();

    QHash<QString, QColor> m_colors;
    bool m_themed = true;
    QString m_source;
    QStringList m_files;
};
