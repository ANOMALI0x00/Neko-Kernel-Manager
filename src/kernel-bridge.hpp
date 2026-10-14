#ifndef KERNEL_BRIDGE_HPP
#define KERNEL_BRIDGE_HPP

#include <QObject>
#include <QStringList>
#include <QVariantList>

class KernelBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage WRITE setStatusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY statusMessageChanged)
    Q_PROPERTY(QString activeKernelVersion READ activeKernelVersion NOTIFY activeKernelVersionChanged)
    Q_PROPERTY(QString detectedCpuLevel READ detectedCpuLevel CONSTANT)
    Q_PROPERTY(QVariantList kernels READ kernels NOTIFY kernelsChanged)
    Q_PROPERTY(QVariantList dkmsModules READ dkmsModules NOTIFY dkmsModulesChanged)
    Q_PROPERTY(QString defaultKernel READ defaultKernel NOTIFY defaultKernelChanged)
    Q_PROPERTY(QString detectedBootloader READ detectedBootloader CONSTANT)
    Q_PROPERTY(QString logs READ logs NOTIFY logsChanged)

public:
    explicit KernelBridge(QObject *parent = nullptr);
    ~KernelBridge();

    QVariantList kernels() const { return m_kernelsCache; }
    QVariantList dkmsModules() const { return m_dkmsCache; }
    QString defaultKernel() const { return m_defaultKernel; }
    QString detectedBootloader() const;

    Q_INVOKABLE void updateKernels();
    Q_INVOKABLE void installKernel(const QString &name);

    // removeKernel: type y version son opcionales. Si no se pasan, la función los deduce.
    // - Si type == "manual", se asume kernel manual (basado en /boot).
    // - Si version está vacía, se intenta extraer del nombre.
    Q_INVOKABLE void removeKernel(const QString &name, const QString &type = "", const QString &version = "");

    Q_INVOKABLE void vkpurge();

    // DKMS Management
    Q_INVOKABLE void updateDkmsModules();
    Q_INVOKABLE void installDkmsModule(const QString &name, const QString &version, const QString &kernel);
    Q_INVOKABLE void removeDkmsModule(const QString &name, const QString &version, const QString &kernel);
    Q_INVOKABLE void autoinstallDkms();

    // Default Kernel Selection
    Q_INVOKABLE void updateDefaultKernel();
    Q_INVOKABLE void setDefaultKernel(const QString &kernelVersion);

    // Log Management
    Q_INVOKABLE void clearLogs() { m_logs = ""; emit logsChanged(); }
    Q_INVOKABLE void exportLogs(const QString &filePath);   // Guarda los logs en un archivo

    // Verbose mode (para depuración)
    void setVerbose(bool v) { m_verbose = v; }
    bool verbose() const { return m_verbose; }

    bool busy() const { return m_busy; }
    int progress() const { return m_progress; }

    QString logs() const { return m_logs; }
    void appendLog(const QString &line);

    QString statusMessage() const { return m_statusMessage; }
    bool statusIsError() const { return m_statusIsError; }
    void setStatusMessage(const QString &message, bool isError = false);
    QString activeKernelVersion() const;
    QString detectedCpuLevel() const;

signals:
    void busyChanged();
    void progressChanged();
    void operationFinished(const QString &message);
    void kernelsChanged();
    void dkmsModulesChanged();
    void defaultKernelChanged();
    void logsChanged();
    void statusMessageChanged();
    void activeKernelVersionChanged();

private:
    bool m_busy = false;
    int m_busyCount = 0;
    int m_progress = 0;
    QString m_logs;
    QVariantList m_kernelsCache;
    QVariantList m_dkmsCache;
    QString m_defaultKernel;

    QVariantList getKernels();
    QVariantList getDkmsModulesInternal();
    QString getDefaultKernelInternal();
    void setBusy(bool b);
    void setProgress(int p) { if(m_progress != p) { m_progress = p; emit progressChanged(); } }

    QString m_statusMessage;
    bool m_statusIsError = false;

    // Modo verbose para depuración avanzada
    bool m_verbose = false;

    Q_DISABLE_COPY_MOVE(KernelBridge)
};

#endif // KERNEL_BRIDGE_HPP