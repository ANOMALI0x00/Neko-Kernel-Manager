#include "theme-manager.hpp"

#include <QFile>
#include <QRegularExpression>

QColor ThemeManager::mix(const QColor &a, const QColor &b, qreal amount) {
    const qreal t = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(
        a.redF()   + (b.redF()   - a.redF())   * t,
        a.greenF() + (b.greenF() - a.greenF()) * t,
        a.blueF()  + (b.blueF()  - a.blueF())  * t,
        a.alphaF() + (b.alphaF() - a.alphaF()) * t
    );
}

static QColor parseValue(QString v, const QHash<QString, QString> &raw, int depth = 0) {
    v = v.trimmed();
    if (v.isEmpty() || depth > 16)
        return QColor();

    if (v.startsWith('@')) {
        const QString ref = v.mid(1).trimmed();
        if (raw.contains(ref))
            return parseValue(raw.value(ref), raw, depth + 1);
        return QColor();
    }

    if (v.startsWith("rgb")) {
        const int l = v.indexOf('('), r = v.indexOf(')');
        if (l >= 0 && r > l) {
            const QStringList parts = v.mid(l + 1, r - l - 1).split(',', Qt::SkipEmptyParts);
            auto chan = [](QString s) -> int {
                s = s.trimmed();
                if (s.endsWith('%'))
                    return qRound(s.left(s.size() - 1).toDouble() * 2.55);
                return qRound(s.toDouble());
            };
            if (parts.size() >= 3) {
                QColor c(qBound(0, chan(parts[0]), 255),
                         qBound(0, chan(parts[1]), 255),
                         qBound(0, chan(parts[2]), 255));
                if (parts.size() >= 4)
                    c.setAlphaF(qBound(0.0, parts[3].trimmed().toDouble(), 1.0));
                return c;
            }
        }
        return QColor();
    }

    return QColor(v);
}

ThemeManager::ThemeManager(QObject *parent) : QObject(parent) {
    load();
}

QColor ThemeManager::get(const QString &name, const QColor &fallback) const {
    const auto it = m_colors.constFind(name);
    if (it != m_colors.constEnd() && it->isValid())
        return *it;
    return fallback;
}

static bool readThemeFileBounded(const QString &path, qint64 maxBytes, QString &outText) {
    if (!path.startsWith(QStringLiteral(":/"))) {
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text) || f.size() > maxBytes) {
        return false;
    }

    const QByteArray data = f.read(maxBytes + 1);
    if (data.size() > maxBytes) {
        return false;
    }
    outText = QString::fromUtf8(data);
    return true;
}

void ThemeManager::load() {
    static const QRegularExpression re(
        QStringLiteral(R"(@define-color\s+([A-Za-z0-9_\-]+)\s+([^;]+);)"));
    static constexpr qint64 kMaxThemeBytes = 1024 * 1024;

    QHash<QString, QString> raw;
    QString text;
    if (readThemeFileBounded(QStringLiteral(":/neko/Data/themes/matecito-dark.css"), kMaxThemeBytes, text)) {
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            raw.insert(m.captured(1), m.captured(2).trimmed());
        }
    }

    // Aliases comunes de tokens GTK3
    if (raw.contains("theme_bg_color") && !raw.contains("window_bg_color"))
        raw.insert("window_bg_color", raw.value("theme_bg_color"));
    if (raw.contains("theme_fg_color") && !raw.contains("window_fg_color"))
        raw.insert("window_fg_color", raw.value("theme_fg_color"));
    if (raw.contains("theme_selected_bg_color") && !raw.contains("accent_bg_color"))
        raw.insert("accent_bg_color", raw.value("theme_selected_bg_color"));
    if (raw.contains("theme_selected_fg_color") && !raw.contains("accent_fg_color"))
        raw.insert("accent_fg_color", raw.value("theme_selected_fg_color"));

    m_colors.clear();
    for (auto it = raw.constBegin(); it != raw.constEnd(); ++it) {
        const QColor c = parseValue(it.value(), raw);
        if (c.isValid())
            m_colors.insert(it.key(), c);
    }
}
