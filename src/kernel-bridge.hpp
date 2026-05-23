#ifndef KERNEL_BRIDGE_HPP
#define KERNEL_BRIDGE_HPP

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QFutureWatcher>
#include "kernel.hpp"

class KernelBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QStringList kernelOrgVersions READ kernelOrgVersions NOTIFY kernelOrgVersionsChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage WRITE setStatusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY statusMessageChanged)
    Q_PROPERTY(QString activeKernelVersion READ activeKernelVersion NOTIFY activeKernelVersionChanged)
    Q_PROPERTY(QVariantList kernels READ kernels NOTIFY kernelsChanged)
    Q_PROPERTY(QString logs READ logs NOTIFY logsChanged)
    Q_PROPERTY(QString targetTemplate READ targetTemplate WRITE setTargetTemplate NOTIFY targetTemplateChanged)
    Q_PROPERTY(QString sourceTemplate READ sourceTemplate WRITE setSourceTemplate NOTIFY sourceTemplateChanged)
    
    Q_PROPERTY(bool lto READ lto WRITE setLto NOTIFY ltoChanged)
    Q_PROPERTY(QString preempt READ preempt WRITE setPreempt NOTIFY preemptChanged)
    Q_PROPERTY(QString cpuOpt READ cpuOpt WRITE setCpuOpt NOTIFY cpuOptChanged)
    Q_PROPERTY(bool zfsSupport READ zfsSupport WRITE setZfsSupport NOTIFY zfsSupportChanged)
    Q_PROPERTY(bool nvidiaSupport READ nvidiaSupport WRITE setNvidiaSupport NOTIFY nvidiaSupportChanged)

public:
    explicit KernelBridge(QObject *parent = nullptr);

    QVariantList kernels() const { return m_kernelsCache; }
    Q_INVOKABLE void updateKernels();
    Q_INVOKABLE void installKernel(const QString &name);
    Q_INVOKABLE void removeKernel(const QString &name);
    Q_INVOKABLE void vkpurge();

    // New Features
    Q_INVOKABLE void fetchKernelOrgVersions(const QString &variant);
    Q_INVOKABLE void downloadKernelOrg(const QString &variant, const QString &version);
    Q_INVOKABLE void cloneCustomGit(const QString &url);
    Q_INVOKABLE void buildKernel(const QString &templateName);
    Q_INVOKABLE void exportPackages(const QString &destPath);
    Q_INVOKABLE void saveConfig();
    Q_INVOKABLE void loadConfig();

    bool busy() const { return m_busy; }
    QStringList kernelOrgVersions() const { return m_kernelOrgVersions; }
    QString targetTemplate() const { return m_targetTemplate; }
    void setTargetTemplate(const QString &t) { if(m_targetTemplate != t) { m_targetTemplate = t; emit targetTemplateChanged(); } }

    QString sourceTemplate() const { return m_sourceTemplate; }
    void setSourceTemplate(const QString &s) { if(m_sourceTemplate != s) { m_sourceTemplate = s; emit sourceTemplateChanged(); } }

    QString logs() const { return m_logs; }
    Q_INVOKABLE void clearLogs() { m_logs = ""; emit logsChanged(); }
    void appendLog(const QString &line);

    bool lto() const { return m_lto; }
    void setLto(bool l) { if(m_lto != l) { m_lto = l; emit ltoChanged(); } }
    
    QString statusMessage() const { return m_statusMessage; }
    bool statusIsError() const { return m_statusIsError; }
    void setStatusMessage(const QString &message, bool isError = false);
    QString activeKernelVersion() const;

    QString preempt() const { return m_preempt; }
    void setPreempt(const QString &p) { if(m_preempt != p) { m_preempt = p; emit preemptChanged(); } }

    QString cpuOpt() const { return m_cpuOpt; }
    void setCpuOpt(const QString &c) { if(m_cpuOpt != c) { m_cpuOpt = c; emit cpuOptChanged(); } }

    bool zfsSupport() const { return m_zfsSupport; }
    void setZfsSupport(bool z) { if(m_zfsSupport != z) { m_zfsSupport = z; emit zfsSupportChanged(); } }

    bool nvidiaSupport() const { return m_nvidiaSupport; }
    void setNvidiaSupport(bool n) { if(m_nvidiaSupport != n) { m_nvidiaSupport = n; emit nvidiaSupportChanged(); } }

signals:
    void busyChanged();
    void operationFinished(const QString &message);
    void kernelsChanged();
    void logsChanged();
    void kernelOrgVersionsChanged();
    void statusMessageChanged();
    void activeKernelVersionChanged();
    void targetTemplateChanged();
    void sourceTemplateChanged();
    void ltoChanged();
    void preemptChanged();
    void cpuOptChanged();
    void zfsSupportChanged();
    void nvidiaSupportChanged();

private:
    bool m_busy = false;
    QStringList m_kernelOrgVersions;
    QString m_targetTemplate = "kernel-neko-rt";
    QString m_sourceTemplate;
    QString m_logs;
    QVariantList m_kernelsCache;
    QVariantList getKernels();
    void setBusy(bool b);

    bool m_lto = false;
    QString m_preempt = "voluntary";
    QString m_cpuOpt = "generic";
    bool m_zfsSupport = false;
    bool m_nvidiaSupport = false;
    QString m_statusMessage;
    bool m_statusIsError = false;
};

#endif // KERNEL_BRIDGE_HPP