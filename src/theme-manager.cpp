#include "theme-manager.hpp"

#include <QFile>
#include <QRegularExpression>

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
    // Solo utilizar la hoja de estilo empaquetada de Matecito Dark
    m_files = {
        ":/neko/Data/themes/matecito-dark.css",
        "Data/themes/matecito-dark.css"
    };

    load();
}

void ThemeManager::reload() {
    load();
    emit changed();
}

QColor ThemeManager::get(const QString &name, const QColor &fallback) const {
    const auto it = m_colors.constFind(name);
    if (it != m_colors.constEnd() && it->isValid())
        return *it;
    return fallback;
}

void ThemeManager::load() {
    static const QRegularExpression re(
        QStringLiteral(R"(@define-color\s+([A-Za-z0-9_\-]+)\s+([^;]+);)"));

    QHash<QString, QString> raw;
    QString foundIn;

    for (const QString &path : m_files) {
        if (!QFile::exists(path)) {
            continue;
        }
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        const QString text = QString::fromUtf8(f.readAll());
        auto it = re.globalMatch(text);
        bool any = false;
        while (it.hasNext()) {
            const auto m = it.next();
            raw.insert(m.captured(1), m.captured(2).trimmed());
            any = true;
        }
        if (any) {
            foundIn = path;
            break; // Se encontró el tema empaquetado, no permitir sobreescrituras
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

    m_themed = !m_colors.isEmpty();
    m_source = foundIn;
}
