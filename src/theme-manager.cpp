#include "theme-manager.hpp"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

// Resolve a raw @define-color value string to a QColor.
//   #rgb / #rrggbb / named  -> QColor directly
//   rgb(...) / rgba(...)     -> parsed by hand
//   @other_token            -> follow the reference (bounded recursion)
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

    return QColor(v); // handles "#rgb", "#rrggbb" and SVG names
}

ThemeManager::ThemeManager(QObject *parent) : QObject(parent) {
    m_dir = QDir::homePath() + "/.config/gtk-4.0";
    // gtk.css is where matugen / pywal / libadwaita write; noctalia.css is what
    // gtk.css @imports for Noctalia. Later files win, matching GTK's load order.
    m_files = { m_dir + "/gtk.css", m_dir + "/noctalia.css" };

    load();

    // Watch the directory (catches atomic replaces) and each file we know about.
    if (QFile::exists(m_dir))
        m_watcher.addPath(m_dir);
    for (const QString &f : m_files)
        if (QFile::exists(f))
            m_watcher.addPath(f);

    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &ThemeManager::onWatch);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &ThemeManager::onWatch);
}

void ThemeManager::onWatch() {
    // Editors/generators replace files atomically, which drops the watch — re-add.
    for (const QString &f : m_files)
        if (QFile::exists(f) && !m_watcher.files().contains(f))
            m_watcher.addPath(f);
    load();
    emit changed();
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

    QHash<QString, QString> raw; // token -> raw value string (later files override)
    QString foundIn;

    for (const QString &path : m_files) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        const QString text = QString::fromUtf8(f.readAll());
        auto it = re.globalMatch(text);
        bool any = false;
        while (it.hasNext()) {
            const auto m = it.next();
            raw.insert(m.captured(1), m.captured(2).trimmed());
            any = true;
        }
        if (any)
            foundIn = path;
    }

    m_colors.clear();
    for (auto it = raw.constBegin(); it != raw.constEnd(); ++it) {
        const QColor c = parseValue(it.value(), raw);
        if (c.isValid())
            m_colors.insert(it.key(), c);
    }

    m_themed = !m_colors.isEmpty();
    m_source = foundIn;
}
