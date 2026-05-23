#ifndef KM_WINDOW_HPP
#define KM_WINDOW_HPP

#include <QMainWindow>
#include <QTreeWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QFutureWatcher>
#include <QThread>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>

#include <functional>
#include <vector>
#include <memory>

#include "conf-window.hpp"

struct Package {
    std::string name;
    std::string version;
    std::string repo;
};

class Kernel {
public:
    Kernel(Package pkg, Package headers);
    std::string version() const;
    bool is_installed() const;
    bool install();
    bool remove();
    std::string category() const;
    std::string name() const { return m_pkg.name; }

    static std::vector<Kernel> getKernels();

private:
    Package m_pkg;
    Package m_headers;
};

class Work : public QObject {
    Q_OBJECT
public:
    using function_t = std::function<void()>;
    explicit Work(function_t func) : m_func(std::move(func)) {}
    ~Work() = default;

public slots:
    void doWork();

private:
    function_t m_func;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onInstall();
    void onRemove();
    void onRefresh();
    void onConfigure();
    void onSourceChanged(int index);
    void onDownload();
    void onWorkFinished();

private:
    void setupUI();
    void populateKernels();
    void commitTransaction();
    void loadKernelOrgVersions();
    void updateDownloadWidgets();

    QTreeWidget *m_treeWidget;
    QPushButton *m_installButton;
    QPushButton *m_removeButton;
    QPushButton *m_refreshButton;
    QPushButton *m_configureButton;
    QComboBox *m_sourceCombo;
    QComboBox *m_variantCombo;
    QComboBox *m_versionCombo;
    QLineEdit *m_customUrlEdit;
    QPushButton *m_downloadButton;
    QProgressBar *m_progressBar;

    std::vector<Kernel> m_kernels;
    std::vector<std::string> m_toInstall;
    std::vector<std::string> m_toRemove;

    std::unique_ptr<ConfWindow> m_confWindow;

    QFutureWatcher<void> *m_watcher;
    QThread *m_workerThread;
    Work *m_worker;
};

#endif // KM_WINDOW_HPP