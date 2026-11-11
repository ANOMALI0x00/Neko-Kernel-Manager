#include "kernel-bridge.hpp"
#include "kernel.hpp"
#include "utils.hpp"
#include <QtConcurrent/QtConcurrent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

static const QString HELPER_PATH = "/usr/libexec/neko-kernel-manager/neko-kernel-helper";

static bool isSafeExecutableFile(const QString &path, bool requireRootOwner) {
    const QByteArray localPath = path.toLocal8Bit();
    struct ::stat st;
    if (::lstat(localPath.constData(), &st) != 0) return false;
    if (!S_ISREG(st.st_mode)) return false;
    if ((st.st_mode & (S_IWGRP | S_IWOTH)) != 0) return false;
    if (requireRootOwner && st.st_uid != 0) return false;
    if (!requireRootOwner && st.st_uid != 0 && st.st_uid != ::getuid()) return false;
    if (::access(localPath.constData(), X_OK) != 0) return false;

    const QFileInfo fi(path);
    struct ::stat dirSt;
    if (::lstat(fi.absolutePath().toLocal8Bit().constData(), &dirSt) == 0) {
        if ((dirSt.st_mode & S_IWOTH) != 0) return false;
    }
    return true;
}

static bool isValidActionName(const QString &action) {
    static const QSet<QString> allowedActions = {
        "install-kernel", "remove-kernel", "reconfigure-kernel", "reconfigure-manual",
        "vkpurge-rm", "vkpurge-rm-all", "manual-remove", "set-default-kernel",
        "dkms-install", "dkms-remove", "dkms-autoinstall", "nvidia-dkms-install"
    };
    return allowedActions.contains(action);
}

static bool isValidPkgName(const QString &pkg) {
    static const QRegularExpression rx("^[a-zA-Z0-9_#+.-]+$");
    return !pkg.isEmpty() && !pkg.startsWith('-') && rx.match(pkg).hasMatch();
}

static bool isValidVersion(const QString &ver) {
    static const QRegularExpression rx("^[a-zA-Z0-9_.-]+$");
    return !ver.isEmpty() && !ver.startsWith('-') && !ver.contains("..") && !ver.contains('/') && rx.match(ver).hasMatch();
}

KernelBridge::KernelBridge(QObject *parent) : QObject(parent), m_verbose(false) {
}

KernelBridge::~KernelBridge() {
    shutdownPrivilegedSession();
}

QString KernelBridge::resolveHelperPath() const {
    if (isSafeExecutableFile(HELPER_PATH, true)) {
        return HELPER_PATH;
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir::cleanPath(appDir + "/../helper/neko-kernel-helper"),
        QDir::cleanPath(appDir + "/helper/neko-kernel-helper")
    };
    for (const QString &candidate : candidates) {
        if (isSafeExecutableFile(candidate, false)) {
            return candidate;
        }
    }
    return QString();
}

void KernelBridge::startPrivilegedSession() {
    if (m_authenticated || m_helperStarting) return;

    const QString helperPath = resolveHelperPath();
    if (helperPath.isEmpty()) {
        appendLog("ERROR: Secure privileged helper not found or failed permission checks.");
        setStatusMessage("Privileged helper not available", true);
        if (m_pendingCallback) {
            auto cb = std::move(m_pendingCallback);
            m_pendingArgs.clear();
            cb(-1);
        }
        return;
    }

    if (!utils::commandExists("pkexec")) {
        appendLog("ERROR: 'pkexec' command not found on system.");
        setStatusMessage("Polkit (pkexec) not found", true);
        if (m_pendingCallback) {
            auto cb = std::move(m_pendingCallback);
            m_pendingArgs.clear();
            cb(-1);
        }
        return;
    }

    const quint64 r1 = QRandomGenerator::global()->generate64();
    const quint64 r2 = QRandomGenerator::global()->generate64();
    m_sessionNonce = QString("%1%2").arg(r1, 16, 16, QChar('0')).arg(r2, 16, 16, QChar('0'));
    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    m_helperStarting = true;

    if (m_helperProcess) {
        m_helperProcess->disconnect(this);
        if (m_helperProcess->state() != QProcess::NotRunning) {
            m_helperProcess->kill();
            m_helperProcess->waitForFinished(500);
        }
        m_helperProcess->deleteLater();
        m_helperProcess = nullptr;
    }

    m_helperProcess = new QProcess(this);
    const QString readyMarker = "__NEKO_READY_" + m_sessionNonce + "__";
    const QString donePrefix = "__NEKO_DONE_" + m_sessionNonce + "__:";

    connect(m_helperProcess, &QProcess::readyReadStandardOutput, this, [this, readyMarker, donePrefix]() {
        m_stdoutBuffer.append(m_helperProcess->readAllStandardOutput());
        int idx;
        while ((idx = m_stdoutBuffer.indexOf('\n')) != -1) {
            QByteArray rawLine = m_stdoutBuffer.left(idx);
            m_stdoutBuffer.remove(0, idx + 1);
            if (!rawLine.isEmpty() && rawLine.endsWith('\r')) rawLine.chop(1);
            const QString line = QString::fromUtf8(rawLine);

            if (line == readyMarker) {
                m_authenticated = true;
                m_helperStarting = false;
                emit authenticatedChanged();
                appendLog("Privileged session authenticated and unlocked for this window.");
                if (!m_pendingArgs.isEmpty() && m_pendingCallback) {
                    const QStringList args = m_pendingArgs;
                    auto cb = std::move(m_pendingCallback);
                    m_pendingArgs.clear();
                    sendHelperCommand(args, std::move(cb));
                }
            } else if (line.startsWith(donePrefix)) {
                bool ok = false;
                int exitCode = line.mid(donePrefix.size()).trimmed().toInt(&ok);
                if (!ok) exitCode = 1;
                m_commandInFlight = false;
                if (m_activeCallback) {
                    auto cb = std::move(m_activeCallback);
                    m_activeCallback = nullptr;
                    cb(exitCode);
                }
            } else if (!line.trimmed().isEmpty()) {
                appendLog(line);
            }
        }
    });

    connect(m_helperProcess, &QProcess::readyReadStandardError, this, [this]() {
        m_stderrBuffer.append(m_helperProcess->readAllStandardError());
        int idx;
        while ((idx = m_stderrBuffer.indexOf('\n')) != -1) {
            QByteArray rawLine = m_stderrBuffer.left(idx);
            m_stderrBuffer.remove(0, idx + 1);
            if (!rawLine.isEmpty() && rawLine.endsWith('\r')) rawLine.chop(1);
            const QString line = QString::fromUtf8(rawLine).trimmed();
            if (!line.isEmpty()) {
                appendLog(line);
            }
        }
    });

    connect(m_helperProcess, &QProcess::started, this, [this]() {
        if (!m_helperProcess) return;
        const QByteArray nonceLine = (m_sessionNonce + '\n').toUtf8();
        if (m_helperProcess->write(nonceLine) == -1) {
            appendLog("ERROR: Failed to transmit session handshake to privileged helper.");
        }
    });

    connect(m_helperProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart) {
            m_helperStarting = false;
            appendLog("ERROR: Failed to launch 'pkexec' privileged session.");
            if (m_commandInFlight && m_activeCallback) {
                m_commandInFlight = false;
                auto cb = std::move(m_activeCallback);
                m_activeCallback = nullptr;
                cb(-1);
            }
            if (m_pendingCallback) {
                auto cb = std::move(m_pendingCallback);
                m_pendingArgs.clear();
                cb(-1);
            }
        }
    });

    connect(m_helperProcess, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus) {
        const bool wasStarting = m_helperStarting;
        m_helperStarting = false;
        if (m_authenticated) {
            m_authenticated = false;
            emit authenticatedChanged();
        }
        if (wasStarting) {
            if (exitCode == 126 || exitCode == 127) {
                appendLog("WARNING: Administrator authentication cancelled. Running in read-only mode until unlocked.");
            } else {
                appendLog(QString("ERROR: Privileged session failed to initialize (exit code %1).").arg(exitCode));
            }
        }
        if (m_commandInFlight && m_activeCallback) {
            m_commandInFlight = false;
            auto cb = std::move(m_activeCallback);
            m_activeCallback = nullptr;
            cb(-1);
        }
        if (m_pendingCallback) {
            auto cb = std::move(m_pendingCallback);
            m_pendingArgs.clear();
            cb(-1);
        }
    });

    appendLog("Requesting administrator authentication via Polkit...");
    m_helperProcess->start("pkexec", {helperPath, "--daemon"});
}

void KernelBridge::shutdownPrivilegedSession() {
    if (m_helperProcess && m_helperProcess->state() != QProcess::NotRunning) {
        m_helperProcess->write("quit\n");
        m_helperProcess->closeWriteChannel();
        if (!m_helperProcess->waitForFinished(1200)) {
            m_helperProcess->terminate();
            if (!m_helperProcess->waitForFinished(500)) {
                m_helperProcess->kill();
            }
        }
    }
    if (m_authenticated) {
        m_authenticated = false;
        emit authenticatedChanged();
    }
    m_helperStarting = false;
}

void KernelBridge::sendHelperCommand(const QStringList &args, std::function<void(int)> callback) {
    if (!m_helperProcess || m_helperProcess->state() != QProcess::Running || !m_authenticated) {
        if (callback) callback(-1);
        return;
    }
    if (args.isEmpty() || args.size() > 32 || !isValidActionName(args.first())) {
        appendLog("SECURITY ERROR: Rejected invalid privileged helper action.");
        if (callback) callback(-1);
        return;
    }
    for (const QString &arg : args) {
        if (arg.contains('\n') || arg.contains('\r') || arg.contains('\t') || arg.contains(QChar('\0'))) {
            appendLog("SECURITY ERROR: Rejected control characters in helper command argument.");
            if (callback) callback(-1);
            return;
        }
    }
    if (m_commandInFlight) {
        appendLog("WARNING: Another privileged operation is already in progress.");
        if (callback) callback(-1);
        return;
    }

    m_commandInFlight = true;
    m_activeCallback = std::move(callback);
    const QByteArray payload = (args.join('\t') + '\n').toUtf8();
    if (m_helperProcess->write(payload) == -1) {
        m_commandInFlight = false;
        auto cb = std::move(m_activeCallback);
        m_activeCallback = nullptr;
        if (cb) cb(-1);
    }
}

void KernelBridge::runPrivilegedAction(const QStringList &args, std::function<void(int)> callback) {
    if (m_authenticated && m_helperProcess && m_helperProcess->state() == QProcess::Running) {
        sendHelperCommand(args, std::move(callback));
        return;
    }
    if (m_pendingCallback) {
        appendLog("WARNING: Another operation is waiting for authentication.");
        if (callback) callback(-1);
        return;
    }
    m_pendingArgs = args;
    m_pendingCallback = std::move(callback);
    startPrivilegedSession();
}

void KernelBridge::setBusy(bool b) {
    if (b) {
        ++m_busyCount;
        if (!m_busy) {
            m_busy = true;
            emit busyChanged();
        }
    } else {
        if (m_busyCount > 0) {
            --m_busyCount;
        }
        if (m_busyCount == 0 && m_busy) {
            m_busy = false;
            emit busyChanged();
            setProgress(0);
        }
    }
}

void KernelBridge::setStatusMessage(const QString &message, bool isError) {
    m_statusMessage = message;
    m_statusIsError = isError;
    emit statusMessageChanged();
}

static QString readActiveKernelVersion() {
    struct ::utsname uts;
    if (::uname(&uts) == 0) {
        return QString::fromLocal8Bit(uts.release).trimmed();
    }
    return QString();
}

QString KernelBridge::activeKernelVersion() const {
    return readActiveKernelVersion();
}

QString KernelBridge::detectedCpuLevel() const {
    return QString::fromStdString(utils::detectCpuLevel());
}

// Void Linux usa GRUB como bootloader por defecto
QString KernelBridge::detectedBootloader() const {
    return "GRUB";
}

static QVariantList buildKernelsList() {
    QVariantList list;
    auto kernels = Kernel::getKernels();
    for (const auto &k : kernels) {
        QVariantMap map;
        map["name"] = QString::fromStdString(k.name());
        map["version"] = QString::fromStdString(k.version());
        map["category"] = QString::fromStdString(k.category());
        map["size"] = QString::fromStdString(k.size());
        map["installDate"] = QString::fromStdString(k.installDate());
        map["installed"] = k.is_installed();
        map["hasFiles"] = k.has_files();
        map["type"] = QString::fromStdString(k.type());
        map["repo"] = QString::fromStdString(k.repo());
        list.append(map);
    }
    return list;
}

void KernelBridge::updateKernels() {
    setBusy(true);
    setProgress(15);
    setStatusMessage("Scanning installed & available kernels...", false);
    appendLog("Scanning system for installed and available kernels...");

    // Registrar los repositorios del sistema activos
    if (utils::commandExists("xbps-query")) {
        std::string reposOut = utils::execProgram("xbps-query", {"-L"});
        if (!reposOut.empty()) {
            appendLog("Active system repositories configured in XBPS:");
            for (const auto &line : utils::split(reposOut, '\n')) {
                std::string t = utils::trim(line);
                if (!t.empty()) {
                    appendLog("  • " + QString::fromStdString(t));
                }
            }
        }
    }

    QPointer<KernelBridge> self(this);
    auto future = QtConcurrent::run([self]() {
        if (!self) return;
        QMetaObject::invokeMethod(self, [self]() {
            if (self) self->setProgress(40);
        });

        auto newList = buildKernelsList();

        QMetaObject::invokeMethod(self, [self, newList]() {
            if (!self) return;
            self->m_kernelsCache = newList;
            self->setProgress(100);
            self->setStatusMessage("Kernels loaded successfully", false);
            self->setBusy(false);
            emit self->kernelsChanged();
            self->appendLog(QString("Kernel scan completed. Found %1 kernel entries:").arg(newList.size()));
            for (const auto &var : newList) {
                QVariantMap m = var.toMap();
                self->appendLog(QString("  • %1 (%2) [Type: %3, Repo: %4, Installed: %5]")
                            .arg(m["name"].toString())
                            .arg(m["version"].toString())
                            .arg(m["type"].toString())
                            .arg(m["repo"].toString())
                            .arg(m["installed"].toBool() ? "Yes" : "No"));
            }
            QTimer::singleShot(1000, self, [self]() {
                if (self) self->setProgress(0);
            });
        });
    });
    (void)future;
}

void KernelBridge::appendLog(const QString &line) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, line]() { appendLog(line); });
        return;
    }

    if (line.contains('\n')) {
        const QStringList parts = line.split('\n', Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            appendLog(part);
        }
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString formattedLine = "[" + timestamp + "] ";

    if (m_verbose) {
        formattedLine += "[VERBOSE] ";
    }

    const QString escapedLine = line.toHtmlEscaped();
    const QString lowerLine = line.toLower();
    if (lowerLine.contains("error") || lowerLine.contains("failed")) {
        formattedLine += "<font color='#ff5555'>" + escapedLine + "</font>";
    } else if (lowerLine.contains("success") || lowerLine.contains("complete") || lowerLine.contains("finished")) {
        formattedLine += "<font color='#50fa7b'>" + escapedLine + "</font>";
    } else if (lowerLine.contains("warning")) {
        formattedLine += "<font color='#f1fa8c'>" + escapedLine + "</font>";
    } else if (line.startsWith("Executing") || line.startsWith("Starting") || line.startsWith("Initiating") || line.startsWith("Scanning") || line.startsWith("Requesting")) {
        formattedLine += "<font color='#bd93f9'>" + escapedLine + "</font>";
    } else {
        formattedLine += escapedLine;
    }

    m_logs += formattedLine + "<br>";
    if (m_logs.length() > 60000) m_logs = m_logs.right(45000);
    emit logsChanged();
}

void KernelBridge::exportLogs(const QString &filePath) {
    if (m_logs.isEmpty()) {
        appendLog("WARNING: No logs to export.");
        setStatusMessage("No logs to export", true);
        return;
    }

    QString cleanPath = filePath;
    if (cleanPath.startsWith("file://")) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }

    QFile file(cleanPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendLog("ERROR: Could not open file for writing: " + cleanPath);
        setStatusMessage("Failed to export logs", true);
        return;
    }

    QTextStream out(&file);
    QString plainLogs = m_logs;
    plainLogs.replace(QRegularExpression("<font[^>]*>"), "");
    plainLogs.replace("</font>", "");
    plainLogs.replace("<br>", "\n");
    plainLogs.replace("&lt;", "<");
    plainLogs.replace("&gt;", ">");
    plainLogs.replace("&quot;", "\"");
    plainLogs.replace("&#39;", "'");
    plainLogs.replace("&amp;", "&");
    out << plainLogs;
    file.close();

    appendLog("Logs exported successfully to: " + cleanPath);
    setStatusMessage("Logs exported to " + cleanPath, false);
}

void KernelBridge::installKernel(const QString &name) {
    if (!isValidPkgName(name)) {
        appendLog("SECURITY ERROR: Invalid kernel package name: " + name);
        setStatusMessage("Invalid package name", true);
        return;
    }

    appendLog("Starting installation of kernel package: " + name);
    setBusy(true);
    setProgress(15);
    setStatusMessage("Installing kernel " + name + "...", false);

    QPointer<KernelBridge> self(this);
    const QString headersPkg = name + "-headers";
    appendLog("Executing helper action: install-kernel " + name + " " + headersPkg);

    runPrivilegedAction({"install-kernel", name, headersPkg}, [self, name](int exitCode) {
        if (!self) return;
        const bool success = (exitCode == 0);
        if (success) {
            self->appendLog("Kernel installation finished successfully: " + name);
            self->setStatusMessage("Kernel installed successfully", false);
            self->setProgress(100);
        } else {
            self->appendLog("ERROR: Main kernel installation failed for " + name);
            self->setStatusMessage("Installation failed", true);
            self->setProgress(0);
        }
        self->setBusy(false);
        self->updateKernels();
        self->updateDkmsModules();
        self->updateDefaultKernel();
    });
}

void KernelBridge::reconfigureKernel(const QString &name, const QString &type, const QString &version) {
    if (!isValidPkgName(name)) {
        appendLog("SECURITY ERROR: Invalid package name: " + name);
        setStatusMessage("Invalid package name", true);
        return;
    }
    if (!version.isEmpty() && !isValidVersion(version)) {
        appendLog("SECURITY ERROR: Invalid kernel version: " + version);
        setStatusMessage("Invalid kernel version", true);
        return;
    }

    appendLog("Initiating kernel reconfiguration (DKMS rebuild & initramfs generation) for: " + name);
    setBusy(true);
    setProgress(15);
    setStatusMessage("Reconfiguring kernel & DKMS modules...", false);

    QPointer<KernelBridge> self(this);
    const bool isManual = (type.toLower() == "manual") || (type.toLower() == "vkpurge");
    const QString ver = version.isEmpty() ? name : version;
    const QStringList args = (!isManual && utils::packageInstalled(name.toStdString()))
        ? QStringList{"reconfigure-kernel", name}
        : QStringList{"reconfigure-manual", name, ver};

    appendLog("Executing helper action: " + args.join(' '));
    runPrivilegedAction(args, [self, name](int exitCode) {
        if (!self) return;
        const bool success = (exitCode == 0);
        if (success) {
            self->appendLog("Kernel reconfiguration completed successfully: " + name);
            self->setStatusMessage("Kernel reconfigured successfully", false);
            self->setProgress(100);
        } else {
            self->appendLog("ERROR: Kernel reconfiguration failed for: " + name);
            self->setStatusMessage("Reconfiguration failed", true);
            self->setProgress(0);
        }
        self->setBusy(false);
        self->updateKernels();
        self->updateDkmsModules();
    });
}

// ----------------------------------------------------------------------------
// removeKernel – CORREGIDA con detección de error por dependencias
// ----------------------------------------------------------------------------
void KernelBridge::removeKernel(const QString &name, const QString &type, const QString &version) {
    if (!isValidPkgName(name)) {
        appendLog("SECURITY ERROR: Invalid kernel package name: " + name);
        setStatusMessage("Invalid package name", true);
        return;
    }
    if (!version.isEmpty() && !isValidVersion(version)) {
        appendLog("SECURITY ERROR: Invalid kernel version: " + version);
        setStatusMessage("Invalid kernel version", true);
        return;
    }

    bool isVkpurge = (type.toLower() == "vkpurge");
    bool isManual = (type.toLower() == "manual") || isVkpurge;
    QString runningVer = activeKernelVersion().trimmed();
    QString targetVersion = version;
    if (targetVersion.isEmpty() && isManual) {
        if (name.startsWith("linux-manual-")) {
            targetVersion = name.mid(QString("linux-manual-").size());
        } else if (name.startsWith("linux-orphan-")) {
            targetVersion = name.mid(QString("linux-orphan-").size());
        }
    }
    if (targetVersion.isEmpty()) {
        targetVersion = name;
    }

    if (!runningVer.isEmpty()) {
        auto matchesBoundary = [](const QString &full, const QString &sub) {
            if (!full.startsWith(sub)) return false;
            if (full.length() == sub.length()) return true;
            QChar next = full.at(sub.length());
            return !next.isLetterOrNumber();
        };
        auto isMatch = [&](const QString &a, const QString &b) {
            if (a.isEmpty() || b.isEmpty()) return false;
            if (a == b) return true;
            return matchesBoundary(a, b) || matchesBoundary(b, a);
        };

        QString cleanName = name;
        cleanName.remove(QRegularExpression("^(linux-?|kernel-)"));

        if (isMatch(runningVer, targetVersion) || isMatch(runningVer, cleanName)) {
            appendLog("SECURITY WARNING: Attempted to uninstall currently running kernel (" + runningVer + "). Operation aborted. Reboot into a different kernel before removing it.");
            setStatusMessage("Cannot remove currently running kernel! Reboot into another kernel first.", true);
            return;
        }
    }

    // Seguridad: no permitir eliminar el único kernel funcional instalado
    int installedCount = 0;
    bool targetHasFiles = true;
    for (const auto &var : m_kernelsCache) {
        QVariantMap m = var.toMap();
        if (m["installed"].toBool() && m["hasFiles"].toBool() && m["type"].toString() != "vkpurge") {
            installedCount++;
        }
        if (m["name"].toString() == name) {
            targetHasFiles = m["hasFiles"].toBool();
        }
    }
    if (installedCount <= 1 && !isVkpurge && targetHasFiles) {
        appendLog("SECURITY ERROR: Cannot remove the only functional installed kernel. Install another kernel first to ensure your system remains bootable.");
        setStatusMessage("Cannot remove the only bootable kernel! Install another kernel first.", true);
        return;
    }

    if (!isManual) {
        std::string pkgName = name.toStdString();
        if (!utils::packageInstalled(pkgName)) {
            appendLog("WARNING: Package " + name + " is not actually installed in XBPS. Switching to manual removal of files.");
            isManual = true;
            setStatusMessage("Package not installed; removing files manually.", false);
        }
    }

    appendLog("Starting removal of kernel: " + name + " (Type: " + QString(isVkpurge ? "vkpurge orphan" : isManual ? "Manual /boot" : "XBPS package") + ")");
    setBusy(true);
    setProgress(10);

    QStringList args;
    if (isVkpurge) {
        args = {"vkpurge-rm", targetVersion};
    } else if (isManual) {
        args = {"manual-remove", targetVersion};
    } else {
        args = {"remove-kernel", name};
        const QString headersPkg = name + "-headers";
        if (utils::packageInstalled(headersPkg.toStdString())) {
            args.append(headersPkg);
            appendLog("Kernel headers package also detected (" + headersPkg + ") and included in removal.");
        }
    }

    QPointer<KernelBridge> self(this);
    appendLog("Executing helper action: " + args.join(' '));
    runPrivilegedAction(args, [self, name](int exitCode) {
        if (!self) return;
        const bool success = (exitCode == 0);
        if (success) {
            self->appendLog("Kernel removal completed successfully: " + name);
            self->setStatusMessage("Kernel removed successfully", false);
            self->setProgress(100);
        } else {
            self->appendLog("ERROR: Kernel removal failed for " + name);
            self->setStatusMessage("Removal failed", true);
            self->setProgress(0);
        }
        self->setBusy(false);
        self->updateKernels();
        self->updateDkmsModules();
        self->updateDefaultKernel();
    });
}

QString KernelBridge::getVkpurgeList() {
    if (!utils::commandExists("vkpurge")) return "";
    std::string out = utils::execProgram("vkpurge", {"list"});
    return QString::fromStdString(out).trimmed();
}

void KernelBridge::vkpurge() {
    appendLog("Initiating vkpurge rm all to remove all old unreferenced kernels...");
    setBusy(true);
    setProgress(30);
    setStatusMessage("Running vkpurge...", false);

    QPointer<KernelBridge> self(this);
    appendLog("Executing helper action: vkpurge-rm-all");
    runPrivilegedAction({"vkpurge-rm-all"}, [self](int exitCode) {
        if (!self) return;
        const bool success = (exitCode == 0);
        self->setBusy(false);
        self->setProgress(success ? 100 : 0);
        if (success) {
            self->appendLog("vkpurge completed successfully.");
            self->setStatusMessage("Old kernels purged successfully", false);
            emit self->operationFinished("Kernels purged");
        } else {
            self->appendLog("ERROR: vkpurge failed.");
            self->setStatusMessage("vkpurge failed", true);
        }
        self->updateKernels();
        self->updateDkmsModules();
        self->updateDefaultKernel();
    });
}

// DKMS Management Implementation
static QVariantList buildDkmsModulesList() {
    QVariantList list;

    auto parseXbpsPackage = [](const std::string &fullPkg, std::string &outName, std::string &outVer) -> bool {
        if (fullPkg.empty()) return false;
        size_t pos = std::string::npos;
        for (size_t i = fullPkg.size(); i > 0; --i) {
            if (fullPkg[i - 1] == '-' && i < fullPkg.size() && std::isdigit(fullPkg[i])) {
                pos = i - 1;
                break;
            }
        }
        if (pos == std::string::npos) return false;
        outName = fullPkg.substr(0, pos);
        outVer = fullPkg.substr(pos + 1);
        return !outName.empty() && !outVer.empty();
    };

    auto normalizeDkmsName = [](const QString &name) -> QString {
        QString normalized = name.toLower();
        if (normalized.endsWith("-dkms")) {
            normalized.chop(5);
            int lastDash = normalized.lastIndexOf('-');
            if (lastDash != -1) {
                QString suffix = normalized.mid(lastDash + 1);
                bool allDigits = true;
                for (int i = 0; i < suffix.size(); ++i) {
                    if (!suffix[i].isDigit()) {
                        allDigits = false;
                        break;
                    }
                }
                if (allDigits) normalized = normalized.left(lastDash);
            }
        }
        return normalized;
    };

    // 1. Get installed/registered DKMS modules from local status
    QMap<QString, QVariantMap> activeModules;
    if (utils::commandExists("dkms")) {
        std::string rawOutput = utils::execProgram("dkms", {"status"}, true);
        if (!rawOutput.empty()) {
            std::vector<std::string> lines = utils::split(rawOutput, '\n');
            for (const auto &line_raw : lines) {
                if (line_raw.empty()) continue;
                QString line = QString::fromStdString(line_raw).trimmed();

                int colonIdx = line.indexOf(':');
                if (colonIdx == -1) continue;

                QString infoPart = line.left(colonIdx).trimmed();
                QString statusPart = line.mid(colonIdx + 1).trimmed();

                QStringList parts = infoPart.split(',');
                if (parts.size() < 2) continue;

                QString modAndVer = parts[0].trimmed();
                QString kernelVer = parts[1].trimmed();
                QString arch = parts.size() >= 3 ? parts[2].trimmed() : "all";

                int slashIdx = modAndVer.indexOf('/');
                QString modName = (slashIdx != -1) ? modAndVer.left(slashIdx) : modAndVer;
                QString modVersion = (slashIdx != -1) ? modAndVer.mid(slashIdx + 1) : "unknown";

                QVariantMap map;
                map["name"] = modName;
                map["version"] = modVersion;
                map["kernel"] = kernelVer;
                map["arch"] = arch;
                map["status"] = statusPart; // e.g. "installed", "built", "added"
                map["isDkmsPackage"] = false; // Registered local module
                map["packageInstalled"] = true;

                activeModules[normalizeDkmsName(modName) + "-" + kernelVer] = map;
                list.append(map);
            }
        }
    }

    // 2. Query installed and available DKMS packages from XBPS repositories in Void Linux
    if (utils::commandExists("xbps-query")) {
        std::string repoXbps = utils::execProgram("xbps-query", {"-Rs", "dkms"});
        std::vector<std::string> xbpsLines = utils::split(repoXbps, '\n');

        std::set<std::string> processedPkgs;

        for (const auto &xbpsLine : xbpsLines) {
            if (xbpsLine.empty()) continue;
            std::istringstream iss(xbpsLine);
            std::string status;
            std::string fullPkg;
            iss >> status >> fullPkg;
            if (fullPkg.empty()) continue;

            std::string pkgName;
            std::string pkgVersion;
            if (!parseXbpsPackage(fullPkg, pkgName, pkgVersion)) continue;

            if (pkgName.find("dkms") == std::string::npos || pkgName == "dkms") continue;
            if (processedPkgs.count(pkgName)) continue;
            processedPkgs.insert(pkgName);

            bool isInstalled = (status == "[*]") || utils::packageInstalled(pkgName);

            QString modName = QString::fromStdString(pkgName);
            QString normalizedPackage = normalizeDkmsName(modName);

            bool alreadyRegistered = false;
            for (auto it = activeModules.constBegin(); it != activeModules.constEnd(); ++it) {
                if (it.key().startsWith(normalizedPackage + "-")) {
                    alreadyRegistered = true;
                    break;
                }
            }

            QVariantMap map;
            map["name"] = QString::fromStdString(pkgName); // Full package name
            map["version"] = QString::fromStdString(pkgVersion);
            map["kernel"] = ""; // Package entries are repo/package-level, not per-registered kernel
            map["arch"] = "all";
            map["status"] = isInstalled ? "unregistered" : "not installed";
            map["isDkmsPackage"] = true; // Flag for package management
            map["packageInstalled"] = isInstalled;
            if (!alreadyRegistered) {
                list.append(map);
            } else if (!isInstalled) {
                list.append(map);
            }
        }
    }

    return list;
}

void KernelBridge::updateDkmsModules() {
    setBusy(true);
    appendLog("Scanning DKMS modules and packages...");
    if (!utils::commandExists("dkms")) {
        appendLog("WARNING: 'dkms' command not found; local DKMS module detection is disabled.");
    }
    if (!utils::commandExists("xbps-query")) {
        appendLog("WARNING: 'xbps-query' command not found; Void DKMS package repository scan is disabled.");
    }

    QPointer<KernelBridge> self(this);
    auto future = QtConcurrent::run([self]() {
        auto dkmsList = buildDkmsModulesList();
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, dkmsList]() {
            if (!self) return;
            self->m_dkmsCache = dkmsList;
            self->setBusy(false);
            emit self->dkmsModulesChanged();
            self->appendLog(QString("DKMS scan completed. Found %1 modules/packages:").arg(dkmsList.size()));
            for (const auto &var : dkmsList) {
                QVariantMap m = var.toMap();
                self->appendLog(QString("  • %1 v%2 (Kernel: %3) [Status: %4]")
                            .arg(m["name"].toString())
                            .arg(m["version"].toString())
                            .arg(m["kernel"].toString())
                            .arg(m["status"].toString()));
            }
        });
    });
    (void)future;
}

void KernelBridge::installDkmsModule(const QString &name, const QString &version, const QString &kernel) {
    if (!isValidPkgName(name)) {
        appendLog("SECURITY ERROR: Invalid DKMS module/package name: " + name);
        setStatusMessage("Invalid module name", true);
        return;
    }
    if (!version.isEmpty() && !isValidVersion(version)) {
        appendLog("SECURITY ERROR: Invalid module version: " + version);
        setStatusMessage("Invalid version", true);
        return;
    }
    if (!kernel.isEmpty() && !isValidVersion(kernel)) {
        appendLog("SECURITY ERROR: Invalid kernel version for DKMS: " + kernel);
        setStatusMessage("Invalid kernel version", true);
        return;
    }

    bool isPackage = name.endsWith("-dkms");
    if (isPackage) {
        appendLog("Starting installation of XBPS DKMS package: " + name);
    } else {
        appendLog("Starting installation of DKMS module: " + name + " v" + version + " for kernel " + kernel);
    }
    setBusy(true);

    QPointer<KernelBridge> self(this);
    const bool isNvidia = name.toLower().contains("nvidia");

    auto finishInstall = [self, isPackage, name](bool success) {
        if (!self) return;
        if (success) {
            self->appendLog((isPackage ? "XBPS DKMS package installation finished: " : "DKMS module installation finished: ") + name);
            self->setStatusMessage(isPackage ? "XBPS DKMS Package installed" : "DKMS module installed", false);
        } else {
            self->appendLog((isPackage ? "ERROR: XBPS DKMS package installation failed: " : "ERROR: DKMS module installation failed: ") + name);
            self->setStatusMessage(isPackage ? "XBPS package installation failed" : "DKMS module installation failed", true);
        }
        self->setBusy(false);
        self->updateDkmsModules();
    };

    const QStringList args = isPackage
        ? QStringList{"install-kernel", name}
        : QStringList{"dkms-install", name, version, kernel};
    appendLog("Executing helper action: " + args.join(' '));

    runPrivilegedAction(args, [self, name, version, isPackage, isNvidia, finishInstall](int exitCode) {
        if (!self) return;
        const bool success = (exitCode == 0);
        if (isPackage) {
            if (success) {
                self->appendLog("XBPS DKMS package installed: " + name);
            } else {
                self->appendLog("ERROR: XBPS DKMS package installation failed for " + name);
            }
        }

        if (!(isNvidia && success && isPackage)) {
            finishInstall(success);
            return;
        }

        std::string nvidiaVersion;
        const std::string srcRoot = "/usr/src";
        if (std::filesystem::exists(srcRoot) && std::filesystem::is_directory(srcRoot)) {
            for (const auto &entry : std::filesystem::directory_iterator(srcRoot)) {
                if (!entry.is_directory()) continue;
                const std::string nameStr = entry.path().filename().string();
                if (nameStr.rfind("nvidia-", 0) == 0) {
                    nvidiaVersion = nameStr.substr(strlen("nvidia-"));
                    break;
                }
            }
        }
        if (nvidiaVersion.empty()) {
            nvidiaVersion = version.toStdString();
            const size_t underscorePos = nvidiaVersion.find('_');
            if (underscorePos != std::string::npos) {
                nvidiaVersion = nvidiaVersion.substr(0, underscorePos);
            }
        }
        if (nvidiaVersion.empty() || !isValidVersion(QString::fromStdString(nvidiaVersion))) {
            self->appendLog("ERROR: Could not determine valid NVIDIA DKMS version.");
            finishInstall(true);
            return;
        }

        std::string modulesRoot = "/usr/lib/modules";
        if (!utils::dirExists(modulesRoot)) {
            modulesRoot = "/lib/modules";
        }
        if (!utils::dirExists(modulesRoot)) {
            self->appendLog("ERROR: No kernel modules directory found for forced NVIDIA DKMS install.");
            finishInstall(true);
            return;
        }

        QStringList dkmsArgs = {"nvidia-dkms-install", QString::fromStdString(nvidiaVersion)};
        for (const auto &entry : std::filesystem::directory_iterator(modulesRoot)) {
            if (!entry.is_directory()) continue;
            const std::string kernelDir = entry.path().filename().string();
            if (kernelDir.empty()) continue;

            std::string versionStr;
            for (char c : kernelDir) {
                if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') versionStr.push_back(c);
                else break;
            }

            bool needsForce = false;
            if (!versionStr.empty()) {
                std::vector<int> parts;
                std::stringstream ss(versionStr);
                std::string part;
                while (std::getline(ss, part, '.')) {
                    if (part.empty()) break;
                    parts.push_back(std::stoi(part));
                }
                if (!parts.empty() && (parts[0] > 6 || (parts[0] == 6 && parts.size() > 1 && parts[1] >= 18))) {
                    needsForce = true;
                }
            }
            if (needsForce && isValidVersion(QString::fromStdString(kernelDir))) {
                dkmsArgs.append(QString::fromStdString(kernelDir));
            }
        }

        if (dkmsArgs.size() <= 2) {
            self->appendLog("No additional kernels required forced NVIDIA DKMS install.");
            finishInstall(true);
            return;
        }

        self->appendLog("Forcing batched NVIDIA DKMS install: " + dkmsArgs.join(' '));
        self->runPrivilegedAction(dkmsArgs, [finishInstall](int) {
            finishInstall(true);
        });
    });
}

void KernelBridge::removeDkmsModule(const QString &name, const QString &version, const QString &kernel) {
    if (!isValidPkgName(name)) {
        appendLog("SECURITY ERROR: Invalid DKMS module/package name: " + name);
        setStatusMessage("Invalid module name", true);
        return;
    }
    if (!version.isEmpty() && !isValidVersion(version)) {
        appendLog("SECURITY ERROR: Invalid module version: " + version);
        setStatusMessage("Invalid version", true);
        return;
    }
    if (!kernel.isEmpty() && !isValidVersion(kernel)) {
        appendLog("SECURITY ERROR: Invalid kernel version: " + kernel);
        setStatusMessage("Invalid kernel version", true);
        return;
    }

    bool isPackage = name.endsWith("-dkms");
    if (isPackage) {
        appendLog("Starting removal of XBPS DKMS package: " + name);
    } else {
        appendLog("Starting removal of DKMS module: " + name + " v" + version + " for kernel " + kernel);
    }
    setBusy(true);

    QStringList args;
    if (isPackage) {
        args = {"remove-kernel", name};
    } else {
        args = {"dkms-remove", name, version};
        if (!kernel.isEmpty()) {
            args.append(kernel);
        }
    }

    QPointer<KernelBridge> self(this);
    appendLog("Executing helper action: " + args.join(' '));
    runPrivilegedAction(args, [self, isPackage, name](int exitCode) {
        if (!self) return;
        const bool success = (exitCode == 0);
        if (success) {
            self->appendLog((isPackage ? "XBPS DKMS package removal finished: " : "DKMS module removal finished: ") + name);
            self->setStatusMessage(isPackage ? "XBPS DKMS Package removed" : "DKMS module removed", false);
        } else {
            self->appendLog((isPackage ? "ERROR: XBPS DKMS package removal failed: " : "ERROR: DKMS module removal failed: ") + name);
            self->setStatusMessage(isPackage ? "XBPS package removal failed" : "DKMS module removal failed", true);
        }
        self->setBusy(false);
        self->updateDkmsModules();
    });
}

void KernelBridge::autoinstallDkms() {
    QString activeK = activeKernelVersion();
    if (activeK.isEmpty() || !isValidVersion(activeK)) {
        appendLog("ERROR: Cannot determine a valid active kernel version for DKMS autoinstall.");
        setStatusMessage("Invalid active kernel version", true);
        return;
    }
    appendLog("Initiating DKMS autoinstall for active kernel: " + activeK);
    setBusy(true);

    QPointer<KernelBridge> self(this);
    appendLog("Executing helper action: dkms-autoinstall " + activeK);
    runPrivilegedAction({"dkms-autoinstall", activeK}, [self](int exitCode) {
        if (!self) return;
        const bool success = (exitCode == 0);
        if (success) {
            self->appendLog("DKMS autoinstall and initramfs update completed successfully.");
            self->setStatusMessage("DKMS autoinstall completed", false);
        } else {
            self->appendLog("ERROR: DKMS autoinstall failed.");
            self->setStatusMessage("DKMS autoinstall failed", true);
        }
        self->setBusy(false);
        self->updateDkmsModules();
    });
}

// Default Kernel Selection Implementation (solo GRUB)
static QString detectDefaultKernel() {
    QFile grubFile("/etc/default/grub");
    if (grubFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString val;
        while (!grubFile.atEnd()) {
            const QString line = QString::fromUtf8(grubFile.readLine()).trimmed();
            if (line.startsWith("GRUB_DEFAULT=")) {
                val = line.mid(QStringLiteral("GRUB_DEFAULT=").size()).trimmed();
                val.remove('"');
                val.remove('\'');
                break;
            }
        }
        if (val == "saved") {
            std::string savedRaw;
            if (utils::commandExists("grub-editenv")) {
                savedRaw = utils::execProgram("grub-editenv", {"list"});
            } else if (utils::commandExists("grub2-editenv")) {
                savedRaw = utils::execProgram("grub2-editenv", {"list"});
            }
            const QString savedOutput = QString::fromUtf8(savedRaw.c_str());
            for (const QString &savedLine : savedOutput.split('\n', Qt::SkipEmptyParts)) {
                const QString trimmed = savedLine.trimmed();
                if (!trimmed.startsWith("saved_entry=")) continue;
                QString savedVal = trimmed.mid(QStringLiteral("saved_entry=").size()).trimmed();
                savedVal.remove('"');
                savedVal.remove('\'');
                if (!savedVal.isEmpty()) {
                    static const QList<QString> markers = {"with Linux ", "con Linux "};
                    for (const QString &marker : markers) {
                        int idx = savedVal.indexOf(marker, 0, Qt::CaseInsensitive);
                        if (idx != -1) {
                            return savedVal.mid(idx + marker.size()).trimmed();
                        }
                    }
                    int arrowIdx = savedVal.lastIndexOf('>');
                    if (arrowIdx != -1) {
                        return savedVal.mid(arrowIdx + 1).trimmed();
                    }
                    return savedVal;
                }
            }
        } else if (!val.isEmpty()) {
            static const QList<QString> markers = {"with Linux ", "con Linux "};
            for (const QString &marker : markers) {
                int withLinuxIdx = val.indexOf(marker, 0, Qt::CaseInsensitive);
                if (withLinuxIdx != -1) {
                    return val.mid(withLinuxIdx + marker.size()).trimmed();
                }
            }
            if (val != "0") {
                return val;
            }
        }
    }
    return readActiveKernelVersion();
}

void KernelBridge::updateDefaultKernel() {
    QPointer<KernelBridge> self(this);
    auto future = QtConcurrent::run([self]() {
        QString def = detectDefaultKernel();
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, def]() {
            if (!self) return;
            self->m_defaultKernel = def;
            emit self->defaultKernelChanged();
            self->appendLog("Current default boot kernel is set to: " + def);
        });
    });
    (void)future;
}

void KernelBridge::setDefaultKernel(const QString &kernelVersion) {
    if (!isValidVersion(kernelVersion)) {
        appendLog("SECURITY ERROR: Invalid kernel version format: " + kernelVersion);
        setStatusMessage("Invalid kernel version", true);
        return;
    }

    appendLog("Requesting default boot kernel change to: " + kernelVersion);
    setBusy(true);
    appendLog("Executing helper action: set-default-kernel " + kernelVersion);

    QPointer<KernelBridge> self(this);
    runPrivilegedAction({"set-default-kernel", kernelVersion}, [self, kernelVersion](int exitCode) {
        if (!self) return;
        const bool success = (exitCode == 0);
        if (success) {
            self->appendLog("Default boot kernel changed to " + kernelVersion + " successfully.");
            self->setStatusMessage("Default kernel updated", false);
            self->m_defaultKernel = kernelVersion;
            emit self->defaultKernelChanged();
        } else {
            self->appendLog("ERROR: Failed to change default kernel to " + kernelVersion);
            self->setStatusMessage("Failed to update default kernel", true);
        }
        self->updateDefaultKernel();
        self->updateKernels();
        self->setBusy(false);
    });
}