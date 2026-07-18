#include "kernel-bridge.hpp"
#include "kernel.hpp"
#include <QtConcurrent/QtConcurrent>
#include <QDateTime>
#include "utils.hpp"
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QTimer>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

KernelBridge::KernelBridge(QObject *parent) : QObject(parent) {
    loadConfig();
    updateKernels();
}

KernelBridge::~KernelBridge() {
    system("rm -rf /tmp/neko-kernel-*");
}

QVariantList KernelBridge::getKernels() {
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
        map["type"] = k.category() == "Custom" ? "manual" : "xbps";
        list.append(map);
    }
    return list;
}

void KernelBridge::updateKernels() {
    setBusy(true);
    setProgress(20);
    setStatusMessage("Fetching kernel list from XBPS...", false);
    
    auto future = QtConcurrent::run([this]() {
        auto newList = getKernels();
        QMetaObject::invokeMethod(this, [this, newList]() {
            m_kernelsCache = newList;
            setBusy(false);
            setProgress(100);
            QTimer::singleShot(1000, this, [this](){ setProgress(0); });
            emit kernelsChanged();
        });
    });
    (void)future;
}

void KernelBridge::appendLog(const QString &line) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString formattedLine = "[" + timestamp + "] ";
    
    if (line.toLower().contains("error") || line.toLower().contains("failed")) {
        formattedLine += "<font color='#ff5555'>" + line + "</font>";
    } else if (line.toLower().contains("success") || line.toLower().contains("complete")) {
        formattedLine += "<font color='#50fa7b'>" + line + "</font>";
    } else if (line.toLower().contains("warning")) {
        formattedLine += "<font color='#f1fa8c'>" + line + "</font>";
    } else if (line.startsWith("Executing:") || line.startsWith("Starting")) {
        formattedLine += "<font color='#bd93f9'>" + line + "</font>";
    } else {
        formattedLine += line;
    }

    m_logs += formattedLine + "<br>";
    if (m_logs.length() > 50000) m_logs = m_logs.right(40000);
    emit logsChanged();
}

void KernelBridge::installKernel(const QString &name) {
    appendLog("DEBUG: installKernel called for: " + name);
    setBusy(true);
    setProgress(10);
    
    auto future = QtConcurrent::run([this, name]() {
        std::string pkgName = name.toStdString();
        std::string headersPkg = pkgName + "-headers";
        
        // Intentamos instalar el kernel primero. Si falla, el proceso se detiene.
        // Luego intentamos instalar los headers. Si fallan, no importa, continuamos.
        std::string cmd = "pkexec sh -c \"xbps-install -y " + pkgName + " && (xbps-install -y " + headersPkg + " || true) && grub-mkconfig -o /boot/grub/grub.cfg\" 2>&1";
        
        appendLog("Executing: " + QString::fromStdString(cmd));

        FILE* pipe = popen(cmd.c_str(), "r");
        bool success = false;
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe)) {
                QString line = QString::fromLocal8Bit(buffer).trimmed();
                QMetaObject::invokeMethod(this, [this, line]() { appendLog(line); });
            }
            success = (pclose(pipe) == 0);
        }

        QMetaObject::invokeMethod(this, [this, name, success]() {
            setBusy(false);
            if (success) {
                appendLog("Installation finished (headers might have been skipped if not found).");
                setStatusMessage("Kernel installed successfully", false);
            } else {
                appendLog("ERROR: Main kernel installation failed.");
                setStatusMessage("Installation failed", true);
            }
            updateKernels();
        });
    });
    (void)future;
}

void KernelBridge::removeKernel(const QString &name) {
    setBusy(true);
    auto future = QtConcurrent::run([this, name]() {
        bool isManual = name.startsWith("linux-manual-");
        std::string cmd;
        if (isManual) {
            std::string ver = name.toStdString().substr(13);
            cmd = "pkexec sh -c \"rm -fv /boot/vmlinuz-" + ver + 
                  " /boot/initramfs-" + ver + ".img && grub-mkconfig -o /boot/grub/grub.cfg\" 2>&1";
        } else {
            std::string pkgName = name.toStdString();
            cmd = "pkexec sh -c \"xbps-remove -Rfy " + pkgName + 
                  " && grub-mkconfig -o /boot/grub/grub.cfg\" 2>&1";
        }
        
        utils::runPrivilegedCommand(cmd);
        QMetaObject::invokeMethod(this, [this]() {
            setBusy(false);
            updateKernels();
        });
    });
    (void)future;
}

void KernelBridge::cleanUninstalledTemplates() {
    appendLog("Cleaning uninstalled custom kernel templates...");
    
    std::string home = utils::getRealHome();
    std::string srcpkgsDir = home + "/.cache/neko-kernel-manager/void-packages/srcpkgs";
    
    if (!utils::dirExists(srcpkgsDir)) {
        appendLog("No template directory found.");
        return;
    }
    
    QDir dir(QString::fromStdString(srcpkgsDir));
    QStringList templates = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    int count = 0;
    for (const QString &tmpl : templates) {
        if (!tmpl.startsWith("kernel-neko-") && !tmpl.startsWith("linux-manual-")) continue;
        
        bool installed = false;
        for (const QVariant &k : m_kernelsCache) {
            if (k.toMap()["name"].toString() == tmpl) {
                if (k.toMap()["installed"].toBool()) {
                    installed = true;
                    break;
                }
            }
        }
        
        if (!installed) {
            utils::runCommand("rm -rf " + srcpkgsDir + "/" + tmpl.toStdString());
            appendLog("Removed template: " + tmpl);
            count++;
        }
    }
    
    appendLog("Cleaned " + QString::number(count) + " uninstalled templates.");
    updateKernels();
}

void KernelBridge::vkpurge() {
    appendLog("Starting vkpurge rm all...");
    setBusy(true);
    setProgress(10);
    setStatusMessage("Purging all old kernels...", false);
    auto future = QtConcurrent::run([this]() {
        std::string cmd = "pkexec sh -c \"vkpurge rm all && grub-mkconfig -o /boot/grub/grub.cfg\" 2>&1";

        QMetaObject::invokeMethod(this, [this]() {
            setProgress(30);
            setStatusMessage("Running vkpurge and updating GRUB...", false);
        });

        FILE* pipe = popen(cmd.c_str(), "r");
        bool success = false;
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe)) {
                QString line = QString::fromLocal8Bit(buffer).trimmed();
                QMetaObject::invokeMethod(this, [this, line]() { 
                    appendLog(line); 
                    if (line.contains("Removing")) setProgress(60);
                    else if (line.contains("Generating grub configuration file")) setProgress(90);
                });
            }
            success = (pclose(pipe) == 0);
        }

        QMetaObject::invokeMethod(this, [this, success]() {
            setBusy(false);
            setProgress(success ? 100 : 0);
            if (success) {
                appendLog("vkpurge completed and GRUB updated.");
                setStatusMessage("Old kernels purged successfully", false);
                emit operationFinished("Kernels purged");
            } else {
                appendLog("vkpurge failed or no kernels to purge.");
                setStatusMessage("vkpurge finished", false);
                emit operationFinished("vkpurge finished");
            }
            updateKernels();
        });
    });
    (void)future;
}

void KernelBridge::fetchKernelOrgVersions(const QString &variant) {
    setBusy(true);
    auto future = QtConcurrent::run([this, variant]() {
        auto versions = utils::fetchKernelOrgVersions(variant.toStdString());
        QStringList qlist;
        for (const auto &v : versions) qlist << QString::fromStdString(v);
        
        QMetaObject::invokeMethod(this, [this, qlist]() {
            m_kernelOrgVersions = qlist;
            setBusy(false);
            emit kernelOrgVersionsChanged();
        });
    });
    (void)future;
}

void KernelBridge::downloadKernelOrg(const QString &variant, const QString &version) {
    setBusy(true);
    setProgress(5);
    auto future = QtConcurrent::run([this, variant, version]() {
        std::string v = version.toStdString();
        std::string var = variant.toStdString();
        std::string pkgName = "kernel-neko-" + var + "-" + v;
        
        QMetaObject::invokeMethod(this, [this, pkgName]() {
            setTargetTemplate(QString::fromStdString(pkgName));
            setStatusMessage("Initializing environment...", false);
            setProgress(10);
        });

        std::string home = utils::getRealHome();
        std::string voidPackagesDir = home + "/.cache/neko-kernel-manager/void-packages";
        
        if (!utils::dirExists(voidPackagesDir)) {
             QMetaObject::invokeMethod(this, [this]() {
                 appendLog("void-packages not found. Cloning repository...");
                 setStatusMessage("Cloning void-packages repository (this may take a while)...", false);
                 setProgress(20);
             });
             utils::runCommand("mkdir -p " + home + "/.cache/neko-kernel-manager");
             bool cloned = utils::gitClone("https://github.com/void-linux/void-packages.git", voidPackagesDir);
              
             if (!cloned || !utils::dirExists(voidPackagesDir)) {
                 QMetaObject::invokeMethod(this, [this, voidPackagesDir]() {
                     appendLog("ERROR: Failed to clone void-packages to " + QString::fromStdString(voidPackagesDir));
                     setStatusMessage("Failed to clone void-packages. Check logs.", true);
                     setBusy(false);
                     setProgress(0);
                 });
                 return;
             }

             QMetaObject::invokeMethod(this, [this]() {
                 appendLog("Bootstrapping xbps-src...");
                 setStatusMessage("Bootstrapping xbps-src...", false);
                 setProgress(50);
             });
             utils::runCommand("cd " + voidPackagesDir + " && ./xbps-src binary-bootstrap");
        }

        QMetaObject::invokeMethod(this, [this, pkgName]() {
            appendLog("Preparing template for " + QString::fromStdString(pkgName) + "...");
            setStatusMessage("Preparing template...", false);
            setProgress(70);
        });
        
        std::string srcpkgsDir = voidPackagesDir + "/srcpkgs/" + pkgName;
        utils::runCommand("mkdir -p " + srcpkgsDir);
        
        std::string url = utils::kernelOrgDownloadUrl(var, v);
        appendLog("Downloading kernel source from: " + QString::fromStdString(url));
        
        std::string ltoStr = m_lto ? "yes" : "no";
        std::string zramTypeStr = m_zramType.toStdString();
        std::string optLevelStr = m_optLevel.toStdString();
        std::string extraFlagsStr = m_extraFlags.toStdString();
        
        appendLog(QString("Generating template with optimizations: CPU=%1, OPT=%2, LTO=%3, ZRAM=%4, EXTRA_FLAGS='%5'")
                  .arg(m_cpuOpt)
                  .arg(m_optLevel)
                  .arg(m_lto ? "Yes" : "No")
                  .arg(m_zramType)
                  .arg(m_extraFlags));

        std::string templateContent = "pkgname=" + pkgName + "\n"
                                     "version=" + v + "\n"
                                     "revision=1\n"
                                     "nodebug=yes\n"
                                     "short_desc=\"Custom " + var + " kernel " + v + " for Void Linux (Neko)\"\n"
                                     "maintainer=\"NekoKernelManager <admin@neko.local>\"\n"
                                     "license=\"GPL-2.0-only\"\n"
                                     "homepage=\"https://www.kernel.org\"\n"
                                     "distfiles=\"" + url + "\"\n"
                                     "checksum=SKIP\n"
                                     "create_wrksrc=yes\n"
                                     "triggers=\"kernel-hooks\"\n"
                                     "kernel_hooks_version=\"" + v + "_1\"\n\n"
                                     "case \"$XBPS_TARGET_MACHINE\" in\n"
                                     "    x86_64*) _arch=x86_64;;\n"
                                     "    i686*) _arch=x86;;\n"
                                     "esac\n\n"
                                     "do_build() {\n"
                                     "    export KCFLAGS=\"-march=" + m_cpuOpt.toStdString() + " -" + optLevelStr + "\"\n"
                                     "    if [ \"" + ltoStr + "\" = \"yes\" ]; then export KCFLAGS=\"$KCFLAGS -flto\"; fi\n"
                                     "    if [ -n \"" + extraFlagsStr + "\" ]; then export KCFLAGS=\"$KCFLAGS " + extraFlagsStr + "\"; fi\n"
                                     "    cd linux-" + v + "\n"
                                     "    make olddefconfig\n"
                                     "    make -j$(nproc)\n"
                                     "}\n\n"
                                     "do_install() {\n"
                                     "    cd linux-" + v + "\n"
                                     "    vmkdir boot\n"
                                     "    vmkdir usr/lib/modules\n"
                                     "    cp arch/${_arch}/boot/bzImage ${DESTDIR}/boot/vmlinuz-${version}_1\n"
                                     "    make modules_install INSTALL_MOD_PATH=${DESTDIR}/usr\n"
                                     "    if [ -d ${DESTDIR}/usr/lib/modules ]; then\n"
                                     "        cp -a ${DESTDIR}/usr/lib/modules/* ${DESTDIR}/usr/lib/modules/ 2>/dev/null || true\n"
                                     "    fi\n"
                                     "    if [ \"" + zramTypeStr + "\" != \"none\" ]; then\n"
                                     "        mkdir -p ${DESTDIR}/etc/modprobe.d\n"
                                     "        cat > ${DESTDIR}/etc/modprobe.d/zram.conf <<'EOF'\n"
                                     "options zram compression=" + zramTypeStr + "\n"
                                     "EOF\n"
                                     "    fi\n"
                                     "}\n";

        QFile file(QString::fromStdString(srcpkgsDir + "/template"));
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << QString::fromStdString(templateContent);
            file.close();
        }

        QMetaObject::invokeMethod(this, [this]() {
            appendLog("Downloading kernel source and generating checksum...");
            setStatusMessage("Downloading kernel...", false);
            setProgress(85);
        });

        std::string distfilesDir = voidPackagesDir + "/hostdir/sources/" + pkgName + "-" + v;
        utils::runCommand("mkdir -p " + distfilesDir);
        std::string destFile = distfilesDir + "/linux-" + v + ".tar.xz";
        
        bool downloadSuccess = utils::downloadFile(url, destFile);
        if (!downloadSuccess) {
             QMetaObject::invokeMethod(this, [this]() {
                 appendLog("ERROR: Failed to download kernel source.");
                 setStatusMessage("Download failed", true);
                 setBusy(false);
                 setProgress(0);
             });
             return;
        }

        std::string sha256 = utils::exec("sha256sum " + destFile + " | cut -d' ' -f1");
        if (sha256.length() != 64) {
             QMetaObject::invokeMethod(this, [this]() {
                 appendLog("ERROR: Failed to generate checksum.");
                 setStatusMessage("Checksum failed", true);
                 setBusy(false);
                 setProgress(0);
             });
             return;
        }

        appendLog("Generated SHA256: " + QString::fromStdString(sha256));
        
        utils::runCommand("sed -i 's/checksum=SKIP/checksum=" + sha256 + "/' " + srcpkgsDir + "/template");

        QMetaObject::invokeMethod(this, [this, pkgName]() {
            appendLog("Kernel prepared successfully: " + QString::fromStdString(pkgName));
            setBusy(false);
            setProgress(100);
            setStatusMessage("Kernel prepared: " + QString::fromStdString(pkgName), false);
            emit operationFinished("Template prepared");
        });
    });
    (void)future;
}

void KernelBridge::cloneCustomGit(const QString &url) {
    setBusy(true);
    auto future = QtConcurrent::run([this, url]() {
        bool isTarball = url.endsWith(".tar.gz") || url.endsWith(".tar.xz") || url.endsWith(".tgz") || url.contains("/releases/download/");
        QString message;
        std::string home = utils::getRealHome();
        std::string voidPackagesDir = home + "/.cache/neko-kernel-manager/void-packages";
        
        std::string pkgName = "kernel-neko-custom";
        if (url.contains("/")) {
            std::string last = url.toStdString().substr(url.toStdString().find_last_of("/") + 1);
            if (last.find(".") != std::string::npos) last = last.substr(0, last.find("."));
            pkgName = "kernel-neko-" + last;
        }

        QMetaObject::invokeMethod(this, [this, pkgName]() {
            setTargetTemplate(QString::fromStdString(pkgName));
        });

        if (!utils::dirExists(voidPackagesDir)) {
             utils::runCommand("mkdir -p " + home + "/.cache/neko-kernel-manager");
             utils::gitClone("https://github.com/void-linux/void-packages.git", voidPackagesDir);
             utils::runCommand("cd " + voidPackagesDir + " && ./xbps-src binary-bootstrap");
        }

        std::string srcpkgsDir = voidPackagesDir + "/srcpkgs/" + pkgName;
        utils::runCommand("mkdir -p " + srcpkgsDir);

        if (isTarball) {
            std::string templateContent = "pkgname=" + pkgName + "\n"
                                         "version=1.0.0\n"
                                         "revision=1\n"
                                         "nodebug=yes\n"
                                         "short_desc=\"Custom precompiled kernel from tarball (Neko)\"\n"
                                         "maintainer=\"NekoKernelManager <admin@neko.local>\"\n"
                                         "license=\"GPL-2.0-only\"\n"
                                         "homepage=\"" + url.toStdString() + "\"\n"
                                         "distfiles=\"" + url.toStdString() + "\"\n"
                                         "checksum=SKIP\n"
                                         "create_wrksrc=yes\n"
                                         "triggers=\"kernel-hooks\"\n"
                                         "kernel_hooks_version=\"1.0.0_1\"\n\n"
                                         "do_install() {\n"
                                         "    vmkdir boot\n"
                                         "    vmkdir usr/lib/modules\n"
                                         "    cp -a boot/* ${DESTDIR}/boot/\n"
                                         "    cp -a lib/modules/* ${DESTDIR}/usr/lib/modules/\n"
                                         "}\n";

            QFile file(QString::fromStdString(srcpkgsDir + "/template"));
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << QString::fromStdString(templateContent);
                file.close();
            }
            
            utils::runCommand("cd " + voidPackagesDir + " && ./xbps-src digest " + pkgName);
            message = "Custom tarball prepared and verified: " + QString::fromStdString(pkgName);
        } else {
            std::string templateContent = "pkgname=" + pkgName + "\n"
                                         "version=git\n"
                                         "revision=1\n"
                                         "nodebug=yes\n"
                                         "short_desc=\"Custom kernel from Git source (Neko)\"\n"
                                         "maintainer=\"NekoKernelManager <admin@neko.local>\"\n"
                                         "license=\"GPL-2.0-only\"\n"
                                         "homepage=\"" + url.toStdString() + "\"\n"
                                         "distfiles=\"" + url.toStdString() + "\"\n"
                                         "create_wrksrc=yes\n"
                                         "triggers=\"kernel-hooks\"\n"
                                         "kernel_hooks_version=\"git_1\"\n\n"
                                         "do_build() {\n"
                                         "    make oldconfig\n"
                                         "    make -j$(nproc)\n"
                                         "}\n\n"
                                         "do_install() {\n"
                                         "    vmkdir boot\n"
                                         "    vmkdir usr/lib/modules\n"
                                         "    cp arch/x86/boot/bzImage ${DESTDIR}/boot/vmlinuz-git_1\n"
                                         "    make modules_install INSTALL_MOD_PATH=${DESTDIR}/usr\n"
                                         "}\n";

            QFile file(QString::fromStdString(srcpkgsDir + "/template"));
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << QString::fromStdString(templateContent);
                file.close();
            }
            message = "Git source template prepared: " + QString::fromStdString(pkgName);
        }
        
        QMetaObject::invokeMethod(this, [this, message]() {
            setBusy(false);
            setStatusMessage(message, false);
            emit operationFinished(message);
            updateKernels();
        });
    });
    (void)future;
}

static std::string resolveKernelSourceTemplate(const std::string &sourceTemplate) {
    if (sourceTemplate == "linux" || sourceTemplate == "linux-lts" || sourceTemplate == "linux-zen" || sourceTemplate == "linux-rt" || sourceTemplate == "linux-mainline") {
        return sourceTemplate;
    }
    if (sourceTemplate.rfind("linux-lts-", 0) == 0) return "linux-lts";
    if (sourceTemplate.rfind("linux-zen-", 0) == 0) return "linux-zen";
    if (sourceTemplate.rfind("linux-rt-", 0) == 0) return "linux-rt";
    if (sourceTemplate.rfind("linux-mainline-", 0) == 0) return "linux-mainline";
    if (sourceTemplate.rfind("linux-", 0) == 0) return "linux";
    return sourceTemplate;
}

void KernelBridge::buildKernel(const QString &templateName) {
    appendLog("DEBUG: buildKernel called for: " + templateName);
    setBusy(true);
    setProgress(5);
    
    auto future = QtConcurrent::run([this, templateName]() {
        std::string home = utils::getRealHome();
        std::string voidPackagesDir = home + "/.cache/neko-kernel-manager/void-packages";
        std::string name = templateName.toStdString();
        std::string sourceTmpl = m_sourceTemplate.toStdString();

        if (!utils::dirExists(voidPackagesDir + "/common")) {
             QMetaObject::invokeMethod(this, [this]() { appendLog("Void-packages environment missing. Setting up bootstrap..."); });
             utils::runCommand("mkdir -p " + home + "/.cache/neko-kernel-manager");
             utils::gitClone("https://github.com/void-linux/void-packages.git", voidPackagesDir);
             utils::runCommand("cd " + voidPackagesDir + " && ./xbps-src binary-bootstrap");
        }

        if (sourceTmpl.empty()) {
            QMetaObject::invokeMethod(this, [this]() {
                appendLog("ERROR: No source kernel selected for template generation.");
                setStatusMessage("Select a source kernel before building.", true);
                setBusy(false);
                setProgress(0);
            });
            return;
        }

        std::string sourceBase = resolveKernelSourceTemplate(sourceTmpl);
        std::string srcPath = voidPackagesDir + "/srcpkgs/" + sourceBase;

        QMetaObject::invokeMethod(this, [this, sourceBase]() {
            appendLog("Using source template: " + QString::fromStdString(sourceBase));
        });

        if (!utils::dirExists(srcPath)) {
            QMetaObject::invokeMethod(this, [this, sourceBase]() {
                appendLog("Source template " + QString::fromStdString(sourceBase) + " not found. Searching for fallback...");
            });
            std::string fallback = utils::exec("ls " + voidPackagesDir + "/srcpkgs | grep '^linux[0-9]\\+\\.[0-9]\\+$' | sort -V | tail -n 1");
            if (!fallback.empty()) {
                QMetaObject::invokeMethod(this, [this, fallback]() {
                    appendLog("Found fallback versioned template: " + QString::fromStdString(fallback));
                });
                srcPath = voidPackagesDir + "/srcpkgs/" + fallback;
                sourceBase = fallback;
            } else if (utils::dirExists(voidPackagesDir + "/srcpkgs/linux")) {
                QMetaObject::invokeMethod(this, [this]() {
                    appendLog("Using default 'linux' template as fallback.");
                });
                srcPath = voidPackagesDir + "/srcpkgs/linux";
                sourceBase = "linux";
            }
        }

        if (!utils::dirExists(srcPath)) {
            QMetaObject::invokeMethod(this, [this, srcPath]() {
                appendLog("ERROR: Source template path not found: " + QString::fromStdString(srcPath));
                setStatusMessage("Official kernel template not found in void-packages.", true);
                setBusy(false);
                setProgress(0);
            });
            return;
        }

        std::string destPath = voidPackagesDir + "/srcpkgs/" + name;

        if (srcPath != destPath) {
            utils::runCommand("rm -rf " + destPath);
            if (!utils::runCommand("cp -r " + srcPath + " " + destPath)) {
                QMetaObject::invokeMethod(this, [this, srcPath]() {
                    appendLog("ERROR: Failed to copy source template from " + QString::fromStdString(srcPath));
                    setStatusMessage("Failed to prepare template directory.", true);
                    setBusy(false);
                    setProgress(0);
                });
                return;
            }
        } else {
            QMetaObject::invokeMethod(this, [this]() {
                appendLog("Target name matches source template. Using existing template without copying.");
            });
        }

        // Usamos el script externo para parchear la plantilla de forma segura
        std::string patchScript = utils::getRealHome() + "/Neko-Kernel-Manager/patch_template.sh";
        if (!utils::runCommand(patchScript + " " + destPath + "/template " + sourceBase + " " + name)) {
             QMetaObject::invokeMethod(this, [this]() {
                appendLog("ERROR: Failed to patch the template file.");
                setStatusMessage("Template patching failed.", true);
                setBusy(false);
                setProgress(0);
            });
            return;
        }

        std::string buildCmd = "env -u XBPS_DISTDIR bash -c \"cd " + voidPackagesDir + " && ./xbps-src pkg " + name + "\" 2>&1";
        
        QMetaObject::invokeMethod(this, [this]() { 
            appendLog("Starting compilation via xbps-src..."); 
            setProgress(15);
        });

        // Movemos QProcess al scope del futuro para que no se destruya prematuramente
        QProcess* process = new QProcess();
        process->setProcessChannelMode(QProcess::MergedChannels);
        process->start(QString::fromStdString(buildCmd));

        while (process->state() == QProcess::Running || process->canReadLine()) {
            if (process->waitForReadyRead(100)) {
                while (process->canReadLine()) {
                    QByteArray data = process->readLine();
                    QString line = QString::fromLocal8Bit(data).trimmed();
                    if (!line.isEmpty()) {
                        QMetaObject::invokeMethod(this, [this, line]() { 
                            appendLog(line); 
                        });
                    }
                }
            }
            QThread::msleep(40);
        }
        
        process->waitForFinished(3000);
        process->deleteLater();
        
        std::string arch = utils::exec("uname -m");
        if (arch.empty()) arch = "x86_64";
        
        // Esperar hasta 60 segundos a que aparezcan los paquetes
        std::string binpkg, headerspkg;
        for (int i = 0; i < 60; ++i) {
            binpkg = utils::exec("find " + voidPackagesDir + "/hostdir/binpkgs -name \"" + name + "-[0-9]*." + arch + ".xbps\" | head -n 1");
            if (!binpkg.empty()) break;
            QThread::msleep(1000);
        }
        
        // Intentar encontrar headers (no bloqueante, puede no existir)
        headerspkg = utils::exec("find " + voidPackagesDir + "/hostdir/binpkgs -name \"" + name + "-headers-[0-9]*." + arch + ".xbps\" | head -n 1");
        
        if (!binpkg.empty()) {
            QMetaObject::invokeMethod(this, [this, binpkg, headerspkg]() {
                appendLog("Success! Binary package generated: " + QString::fromStdString(binpkg));
                appendLog("Headers package: " + QString::fromStdString(headerspkg));
                setStatusMessage("Build finished. Package available in void-packages/hostdir/binpkgs.", false);
            });
        } else {
            QMetaObject::invokeMethod(this, [this]() {
                appendLog("ERROR: compilation finished but target .xbps package was not found. Inspect the log tracking above.");
                setStatusMessage("Build failed: package not found.", true);
            });
        }
        
        QMetaObject::invokeMethod(this, [this]() { 
            setBusy(false); 
            setProgress(100);
        });
    });
}

void KernelBridge::saveConfig() {}
void KernelBridge::loadConfig() {}
void KernelBridge::setStatusMessage(const QString &m, bool e) { m_statusMessage = m; m_statusIsError = e; emit statusMessageChanged(); }
void KernelBridge::setBusy(bool b) { m_busy = b; emit busyChanged(); }
QString KernelBridge::activeKernelVersion() const { return QString::fromStdString(utils::exec("uname -r")); }
QString KernelBridge::detectedCpuLevel() const { return QString::fromStdString(utils::detectCpuLevel()); }
void KernelBridge::exportPackages(const QString &d) { Q_UNUSED(d); }