#include "settingdialog.h"
#include "ui_settingdialog.h"
#include <QAbstractButton>
#include <QtDebug>
SettingDialog::SettingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingDialog)
{
    ui->setupUi(this);
    setFixedSize(size());

    connect(ui->buttonBox, &QDialogButtonBox::clicked, [=](QAbstractButton* button) {
        if (button->text() == "Apply")
            emit apply(ui->spin_gap->value(),
                       ui->spin_dist->value(),
                       ui->spin_anitime->value(),
                       ui->spin_maxsize->value());
    });
}

void SettingDialog::setValue(int gap, int dist, int anitime, int maxsize)
{
    ui->spin_gap->setValue(gap);
    ui->spin_dist->setValue(dist);
    ui->spin_anitime->setValue(anitime);
    ui->spin_maxsize->setValue(maxsize);
}

SettingDialog::~SettingDialog()
{
    delete ui;
}
