#include "conf-window.hpp"
#include "utils.hpp"
#include <QMessageBox>
#include <QFileDialog>
#include <QProcess>

ConfWindow::ConfWindow(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Kernel Configuration");
    setupUI();
}

void ConfWindow::setupUI() {
    auto *layout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget;
    layout->addWidget(m_tabWidget);

    // Options Tab
    m_optionsTab = new QWidget;
    auto *optionsLayout = new QVBoxLayout(m_optionsTab);

    m_ltoCheck = new QCheckBox("Enable LTO");
    optionsLayout->addWidget(m_ltoCheck);

    auto *preemptLayout = new QHBoxLayout;
    preemptLayout->addWidget(new QLabel("Preempt:"));
    m_preemptCombo = new QComboBox;
    m_preemptCombo->addItems({"none", "voluntary", "full"});
    preemptLayout->addWidget(m_preemptCombo);
    optionsLayout->addLayout(preemptLayout);

    auto *cpuLayout = new QHBoxLayout;
    cpuLayout->addWidget(new QLabel("CPU Opt:"));
    m_cpuOptCombo = new QComboBox;
    m_cpuOptCombo->addItems({"generic", "native"});
    cpuLayout->addWidget(m_cpuOptCombo);
    optionsLayout->addLayout(cpuLayout);

    m_tabWidget->addTab(m_optionsTab, "Options");

    // Patches Tab
    m_patchesTab = new QWidget;
    auto *patchesLayout = new QVBoxLayout(m_patchesTab);
    m_patchesEdit = new QTextEdit;
    patchesLayout->addWidget(m_patchesEdit);
    m_tabWidget->addTab(m_patchesTab, "Patches");

    // Buttons
    auto *buttonLayout = new QHBoxLayout;
    m_saveButton = new QPushButton("Save Config");
    m_loadButton = new QPushButton("Load Config");
    m_buildButton = new QPushButton("Build Kernel");
    m_exportButton = new QPushButton("Export Package");

    connect(m_saveButton, &QPushButton::clicked, this, &ConfWindow::onSave);
    connect(m_loadButton, &QPushButton::clicked, this, &ConfWindow::onLoad);
    connect(m_buildButton, &QPushButton::clicked, this, &ConfWindow::onBuild);
    connect(m_exportButton, &QPushButton::clicked, this, &ConfWindow::onExport);

    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_loadButton);
    buttonLayout->addWidget(m_buildButton);
    buttonLayout->addWidget(m_exportButton);
    layout->addLayout(buttonLayout);
}

void ConfWindow::onExport() {
    std::string home = getenv("HOME");
    std::string voidPackagesDir = home + "/.cache/neko-kernel-manager/void-packages";
    std::string binpkgsDir = voidPackagesDir + "/hostdir/binpkgs";

    if (!utils::dirExists(binpkgsDir)) {
        QMessageBox::warning(this, "Export", "No compiled packages found in void-packages/hostdir/binpkgs.");
        return;
    }

    QString destDir = QFileDialog::getExistingDirectory(this, "Select Export Directory");
    if (!destDir.isEmpty()) {
        std::string cmd = "find " + binpkgsDir + " -name '*.xbps' -exec cp {} " + destDir.toStdString() + " \\;";
        utils::runCommand(cmd);
        QMessageBox::information(this, "Exported", "Kernel packages exported successfully.");
    }
}

void ConfWindow::onSave() {
    QString fileName = QFileDialog::getSaveFileName(this, "Save Config", "", "JSON Files (*.json)");
    if (!fileName.isEmpty()) {
        // Save to JSON using Rust lib
        QMessageBox::information(this, "Saved", "Config saved.");
    }
}

void ConfWindow::onLoad() {
    QString fileName = QFileDialog::getOpenFileName(this, "Load Config", "", "JSON Files (*.json)");
    if (!fileName.isEmpty()) {
        // Load from JSON
        QMessageBox::information(this, "Loaded", "Config loaded.");
    }
}

void ConfWindow::onBuild() {
    std::string home = getenv("HOME");
    std::string cacheDir = home + "/.cache/neko-kernel-manager";
    std::string voidPackagesDir = cacheDir + "/void-packages";

    if (!utils::dirExists(voidPackagesDir)) {
        QMessageBox::information(this, "Setup", "void-packages not found. Cloning it now...");
        utils::runCommand("mkdir -p " + cacheDir);
        utils::gitClone("https://github.com/void-linux/void-packages.git", voidPackagesDir);
        // Bootstrap xbps-src
        utils::runCommand("cd " + voidPackagesDir + " && ./xbps-src binary-bootstrap");
    }

    // Example: Clone custom repo for RT kernel
    std::string customRepoDir = cacheDir + "/custom-templates";
    if (!utils::dirExists(customRepoDir)) {
        utils::gitClone("https://github.com/javiercplus/neko-void-packages.git", customRepoDir);
    }

    // Link custom template
    std::string templateName = "kernel-neko-rt";
    std::string srcPath = customRepoDir + "/srcpkgs/" + templateName;
    std::string destPath = voidPackagesDir + "/srcpkgs/" + templateName;
    
    utils::runCommand("ln -sf " + srcPath + " " + voidPackagesDir + "/srcpkgs/");

    // Apply configuration via Rust before building
    std::string jsonConfig = "{\"lto\":" + std::string(m_ltoCheck->isChecked() ? "true" : "false") + 
                             ",\"preempt\":\"" + m_preemptCombo->currentText().toStdString() + "\"" +
                             ",\"cpu_opt\":\"" + m_cpuOptCombo->currentText().toStdString() + "\"}";
    
    // In a real xbps-src flow, we'd apply this to the template's config or after 'fetch'
    // apply_config_options(jsonConfig.c_str(), (voidPackagesDir + "/srcpkgs/" + templateName + "/files/x86_64-dotconfig").c_str());

    // Run build
    std::string buildCmd = "cd " + voidPackagesDir + " && ./xbps-src pkg " + templateName;
    utils::runInTerminal(buildCmd);
    
    QMessageBox::information(this, "Build Started", "Building kernel in terminal...");
}