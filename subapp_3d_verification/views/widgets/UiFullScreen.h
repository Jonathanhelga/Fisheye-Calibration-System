#pragma once
// Auto-generated from pyuic by cpp/tools/pyuic_to_cpp.py — do not edit by hand.
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QFrame>
#include <QScrollArea>
#include <QTabWidget>
#include <QToolBox>
#include <QGroupBox>
#include <QTextBrowser>
#include <QSlider>
#include <QProgressBar>
#include <QStackedWidget>
#include <QBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QCursor>
#include <QFont>
#include <QCoreApplication>
#include <QRect>
#include <QSize>
#include <QPoint>
#include <QDialog>

namespace Ui {
class Form {
public:
    QHBoxLayout *horizontalLayout;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QHBoxLayout *horizontalLayout_2;
    QLabel *label_full_screen;

    void setupUi(QWidget *w) {
        w->setObjectName("Form");
        w->resize(1094, 829);
        w->setMinimumSize(QSize(800, 600));
        horizontalLayout = new QHBoxLayout(w);
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        horizontalLayout->setSpacing(0);
        horizontalLayout->setObjectName("horizontalLayout");
        scrollArea = new QScrollArea(w);
        scrollArea->setWidgetResizable(false);
        scrollArea->setAlignment(Qt::AlignCenter);
        scrollArea->setObjectName("scrollArea");
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setGeometry(QRect(65, 114, 962, 598));
        QSizePolicy sizePolicy1(QSizePolicy::Ignored, QSizePolicy::Ignored);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(scrollAreaWidgetContents->sizePolicy().hasHeightForWidth());
        scrollAreaWidgetContents->setSizePolicy(sizePolicy1);
        scrollAreaWidgetContents->setMinimumSize(QSize(1, 1));
        scrollAreaWidgetContents->setMouseTracking(false);
        scrollAreaWidgetContents->setObjectName("scrollAreaWidgetContents");
        horizontalLayout_2 = new QHBoxLayout(scrollAreaWidgetContents);
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        label_full_screen = new QLabel(scrollAreaWidgetContents);
        label_full_screen->setEnabled(true);
        QSizePolicy sizePolicy2(QSizePolicy::Ignored, QSizePolicy::Ignored);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(label_full_screen->sizePolicy().hasHeightForWidth());
        label_full_screen->setSizePolicy(sizePolicy2);
        label_full_screen->setMinimumSize(QSize(1, 1));
        label_full_screen->setMouseTracking(true);
        label_full_screen->setFocusPolicy(Qt::StrongFocus);
        label_full_screen->setFrameShape(QFrame::NoFrame);
        label_full_screen->setText("");
        label_full_screen->setObjectName("label_full_screen");
        horizontalLayout_2->addWidget(label_full_screen);
        scrollArea->setWidget(scrollAreaWidgetContents);
        horizontalLayout->addWidget(scrollArea);
        retranslateUi(w);
        QMetaObject::connectSlotsByName(w);
    }

    void retranslateUi(QWidget *w) {
        w->setWindowTitle(QCoreApplication::translate("Form", "Form"));
    }
};
}  // namespace Ui
