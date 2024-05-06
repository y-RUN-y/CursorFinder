#ifndef SETTINGDIALOG_H
#define SETTINGDIALOG_H

#include <QDialog>

namespace Ui {
class SettingDialog;
}

class SettingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingDialog(QWidget *parent = nullptr);
    void setValue(int gap, int dist, int anitime, int maxsize);
    ~SettingDialog();

signals:
    void apply(int gap, int distance, int anitime, int maxsize);

private:
    Ui::SettingDialog *ui;
};

#endif // SETTINGDIALOG_H
