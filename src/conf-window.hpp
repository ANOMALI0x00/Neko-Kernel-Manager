#ifndef CONF_WINDOW_HPP
#define CONF_WINDOW_HPP

#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>

class ConfWindow : public QDialog {
    Q_OBJECT

public:
    ConfWindow(QWidget *parent = nullptr);
    ~ConfWindow() = default;

private slots:
    void onSave();
    void onLoad();
    void onBuild();
    void onExport();

private:
    void setupUI();

    QTabWidget *m_tabWidget;
    QWidget *m_optionsTab;
    QWidget *m_patchesTab;

    // Options
    QCheckBox *m_ltoCheck;
    QComboBox *m_preemptCombo;
    QComboBox *m_cpuOptCombo;

    // Patches
    QTextEdit *m_patchesEdit;

    QPushButton *m_saveButton;
    QPushButton *m_loadButton;
    QPushButton *m_buildButton;
    QPushButton *m_exportButton;
};

#endif // CONF_WINDOW_HPP