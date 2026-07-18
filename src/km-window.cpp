#include "km-window.hpp"
#include "utils.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QTreeWidgetItem>
#include <QMessageBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QLabel>
#include <QtConcurrent/QtConcurrent>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Neko Kernel Manager");
    setupUI();

    m_confWindow = std::make_unique<ConfWindow>(this);

    m_watcher = new QFutureWatcher<void>(this);
    connect(m_watcher, &QFutureWatcher<void>::finished, this, &MainWindow::onWorkFinished);

    m_kernelWatcher = new QFutureWatcher<std::vector<Kernel>>(this);
    connect(m_kernelWatcher, &QFutureWatcher<std::vector<Kernel>>::finished, this, &MainWindow::onKernelsLoaded);

    loadKernelList();
}

MainWindow::~MainWindow() {
    // Resources are parented; nothing special to do here.
}

void MainWindow::setupUI() {
    auto *centralWidget = new QWidget;
    setCentralWidget(centralWidget);

    auto *layout = new QVBoxLayout(centralWidget);

    auto *downloadGroup = new QGroupBox("Kernel sources");
    auto *downloadLayout = new QFormLayout(downloadGroup);
    m_sourceCombo = new QComboBox;
    m_sourceCombo->addItems({"Void repo", "kernel.org", "Custom Git"});
    m_variantCombo = new QComboBox;
    m_variantCombo->addItems({"linux", "linux-lts", "linux-rt", "stable", "lts"});
    m_versionCombo = new QComboBox;
    m_customUrlEdit = new QLineEdit;
    m_customUrlEdit->setPlaceholderText("https://github.com/user/linux-template.git");
    m_downloadButton = new QPushButton("Download / Install");

    connect(m_sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onSourceChanged);
    connect(m_variantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::loadKernelOrgVersions);
    connect(m_downloadButton, &QPushButton::clicked, this, &MainWindow::onDownload);

    downloadLayout->addRow(new QLabel("Source:"), m_sourceCombo);
    downloadLayout->addRow(new QLabel("Variant:"), m_variantCombo);
    downloadLayout->addRow(new QLabel("Version:"), m_versionCombo);
    downloadLayout->addRow(new QLabel("Custom repo:"), m_customUrlEdit);
    downloadLayout->addRow(m_downloadButton);
    layout->addWidget(downloadGroup);

    updateDownloadWidgets();

    m_treeWidget = new QTreeWidget;
    m_treeWidget->setColumnCount(4);
    m_treeWidget->setHeaderLabels({"Package", "Version", "Category", "Installed"});
    m_treeWidget->header()->setStretchLastSection(true);
    layout->addWidget(m_treeWidget);

    auto *buttonLayout = new QHBoxLayout;
    m_installButton = new QPushButton("Install");
    m_removeButton = new QPushButton("Remove");
    m_refreshButton = new QPushButton("Refresh");
    m_configureButton = new QPushButton("Configure");

    connect(m_installButton, &QPushButton::clicked, this, &MainWindow::onInstall);
    connect(m_removeButton, &QPushButton::clicked, this, &MainWindow::onRemove);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::onRefresh);
    connect(m_configureButton, &QPushButton::clicked, this, &MainWindow::onConfigure);

    buttonLayout->addWidget(m_installButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addWidget(m_configureButton);
    layout->addLayout(buttonLayout);

    m_progressBar = new QProgressBar;
    m_progressBar->setVisible(false);
    layout->addWidget(m_progressBar);
}

void MainWindow::loadKernelList() {
    m_treeWidget->clear();
    m_treeWidget->setEnabled(false);
    m_progressBar->setVisible(true);
    m_kernelWatcher->setFuture(QtConcurrent::run(&Kernel::getKernels));
}

void MainWindow::populateKernels(const std::vector<Kernel> &kernels) {
    m_treeWidget->clear();
    m_kernels = kernels;

    if (m_kernels.empty() && !utils::commandExists("xbps-query")) {
        QMessageBox::warning(this, "Missing xbps", "xbps-query is not installed or not available in PATH. Void kernel management is disabled.");
    }

    for (const auto &kernel : m_kernels) {
        auto *item = new QTreeWidgetItem(m_treeWidget);
        item->setText(0, QString::fromStdString(kernel.name()));
        item->setText(1, QString::fromStdString(kernel.version()));
        item->setText(2, QString::fromStdString(kernel.category()));
        item->setText(3, kernel.is_installed() ? "Yes" : "No");
        item->setCheckState(0, Qt::Unchecked);
    }
}

void MainWindow::onKernelsLoaded() {
    std::vector<Kernel> kernels = m_kernelWatcher->future().result();
    m_treeWidget->setEnabled(true);
    m_progressBar->setVisible(false);
    populateKernels(kernels);
}

void MainWindow::onInstall() {
    m_toInstall.clear();
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        auto *item = m_treeWidget->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked && item->text(3) == "No") {
            m_toInstall.push_back(item->text(0).toStdString());
        }
    }
    if (!m_toInstall.empty()) {
        m_progressBar->setVisible(true);
        m_watcher->setFuture(QtConcurrent::run([this]() { commitTransaction(); }));
    }
}

void MainWindow::onRemove() {
    m_toRemove.clear();
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        auto *item = m_treeWidget->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked && item->text(3) == "Yes") {
            m_toRemove.push_back(item->text(0).toStdString());
        }
    }
    if (!m_toRemove.empty()) {
        m_progressBar->setVisible(true);
        m_watcher->setFuture(QtConcurrent::run([this]() { commitTransaction(); }));
    }
}

void MainWindow::onRefresh() {
    loadKernelList();
}

void MainWindow::onConfigure() {
    m_confWindow->show();
}

void MainWindow::onSourceChanged(int index) {
    Q_UNUSED(index)
    loadKernelOrgVersions();
    updateDownloadWidgets();
}

void MainWindow::updateDownloadWidgets() {
    const bool kernelOrg = m_sourceCombo->currentText() == "kernel.org";
    const bool customGit = m_sourceCombo->currentText() == "Custom Git";

    m_versionCombo->setEnabled(kernelOrg);
    m_variantCombo->setEnabled(true);
    m_customUrlEdit->setEnabled(customGit);
    m_downloadButton->setText(kernelOrg ? "Download from kernel.org" : customGit ? "Clone custom repo" : "Install from Void repo");
}

void MainWindow::loadKernelOrgVersions() {
    if (m_sourceCombo->currentText() != "kernel.org") {
        return;
    }
    std::string variant = m_variantCombo->currentText().toStdString();
    if (variant == "linux") {
        variant = "stable";
    } else if (variant == "linux-lts") {
        variant = "lts";
    } else if (variant == "linux-rt") {
        variant = "stable";
    }
    const auto versions = utils::fetchKernelOrgVersions(variant);
    m_versionCombo->clear();
    for (const auto &ver : versions) {
        m_versionCombo->addItem(QString::fromStdString(ver));
    }
}

void MainWindow::onDownload() {
    const QString source = m_sourceCombo->currentText();
    if (source == "Void repo") {
        const QString variant = m_variantCombo->currentText();
        const QString cmd = QString("xbps-install -y %1").arg(variant);
        utils::runCommand(cmd.toStdString());
        QMessageBox::information(this, "Install", QStringLiteral("Installed %1 from Void repos.").arg(variant));
        loadKernelList();
        return;
    }

    if (source == "kernel.org") {
        const QString variant = m_variantCombo->currentText();
        const QString version = m_versionCombo->currentText();
        if (version.isEmpty()) {
            QMessageBox::warning(this, "Version missing", "Select a kernel.org version.");
            return;
        }
        const std::string dest = "/tmp/neko-kernel-" + version.toStdString();
        const std::string url = utils::kernelOrgDownloadUrl(variant.toStdString(), version.toStdString());
        const bool ok = utils::downloadFile(url, dest + "/kernel.tar.xz");
        if (ok) {
            QMessageBox::information(this, "Downloaded", QString::fromStdString(std::string("Downloaded ") + url + " to " + dest));
        } else {
            QMessageBox::critical(this, "Download failed", "Failed to download kernel.org tarball.");
        }
        return;
    }

    if (source == "Custom Git") {
        const QString url = m_customUrlEdit->text();
        if (url.isEmpty()) {
            QMessageBox::warning(this, "URL missing", "Enter a custom Git repository URL.");
            return;
        }
        const std::string dest = "/tmp/neko-kernel-git";
        const bool ok = utils::gitClone(url.toStdString(), dest);
        if (ok) {
            QMessageBox::information(this, "Cloned", QString::fromStdString(std::string("Cloned repository to ") + dest));
        } else {
            QMessageBox::critical(this, "Clone failed", "Failed to clone custom Git repository.");
        }
        return;
    }
}

void MainWindow::onWorkFinished() {
    m_progressBar->setVisible(false);
    loadKernelList();
    QMessageBox::information(this, "Done", "Operation completed.");
}
void MainWindow::commitTransaction() {
    if (!m_toInstall.empty()) {
        std::vector<std::string> fullInstallList;
        for (const auto& pkg : m_toInstall) {
            fullInstallList.push_back(pkg);
            std::string headersPkg = pkg + "-headers";
            if (utils::packageExists(headersPkg)) {
                fullInstallList.push_back(headersPkg);
            }
        }
        std::string cmd = "pkexec xbps-install -y " + utils::join(fullInstallList, " ");
        utils::runCommand(cmd);
    }
    if (!m_toRemove.empty()) {
        std::vector<std::string> fullRemoveList;
        for (const auto& pkg : m_toRemove) {
            fullRemoveList.push_back(pkg);
            std::string headersPkg = pkg + "-headers";
            if (utils::packageInstalled(headersPkg)) {
                fullRemoveList.push_back(headersPkg);
            }
        }
        std::string cmd = "pkexec xbps-remove -y " + utils::join(fullRemoveList, " ");
        utils::runCommand(cmd);
    }
    m_toInstall.clear();
    m_toRemove.clear();
}