/********************************************************************************
** Form generated from reading UI file 'classify.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CLASSIFY_H
#define UI_CLASSIFY_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Classify
{
public:
    QWidget *verticalLayoutWidget;
    QVBoxLayout *verticalLayout;
    QGridLayout *gridLayout_2;
    QFrame *line_3;
    QPushButton *pushButton_3;
    QVBoxLayout *verticalLayout_3;
    QListView *listView;
    QLabel *label_2;
    QLabel *label_6;
    QPushButton *pushButton_2;
    QLabel *label_4;
    QFrame *line;
    QPushButton *pushButton_4;
    QVBoxLayout *verticalLayout_2;
    QScrollArea *scrollArea_2;
    QWidget *scrollAreaWidgetContents_2;
    QPushButton *pushButton;
    QLabel *label_5;
    QLabel *label_3;
    QLabel *label;
    QVBoxLayout *verticalLayout_4;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QPushButton *pushButton_5;
    QSpacerItem *horizontalSpacer_2;
    QFrame *line_2;

    void setupUi(QWidget *Classify)
    {
        if (Classify->objectName().isEmpty())
            Classify->setObjectName(QString::fromUtf8("Classify"));
        Classify->resize(1688, 892);
        verticalLayoutWidget = new QWidget(Classify);
        verticalLayoutWidget->setObjectName(QString::fromUtf8("verticalLayoutWidget"));
        verticalLayoutWidget->setGeometry(QRect(0, 0, 1691, 891));
        verticalLayout = new QVBoxLayout(verticalLayoutWidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        gridLayout_2 = new QGridLayout();
        gridLayout_2->setObjectName(QString::fromUtf8("gridLayout_2"));
        gridLayout_2->setVerticalSpacing(6);
        gridLayout_2->setContentsMargins(-1, 1, -1, -1);
        line_3 = new QFrame(verticalLayoutWidget);
        line_3->setObjectName(QString::fromUtf8("line_3"));
        line_3->setFrameShape(QFrame::VLine);
        line_3->setFrameShadow(QFrame::Sunken);

        gridLayout_2->addWidget(line_3, 4, 4, 2, 1);

        pushButton_3 = new QPushButton(verticalLayoutWidget);
        pushButton_3->setObjectName(QString::fromUtf8("pushButton_3"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(pushButton_3->sizePolicy().hasHeightForWidth());
        pushButton_3->setSizePolicy(sizePolicy);
        pushButton_3->setMinimumSize(QSize(402, 0));

        gridLayout_2->addWidget(pushButton_3, 2, 0, 1, 1);

        verticalLayout_3 = new QVBoxLayout();
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        listView = new QListView(verticalLayoutWidget);
        listView->setObjectName(QString::fromUtf8("listView"));
        listView->setMinimumSize(QSize(400, 0));

        verticalLayout_3->addWidget(listView);


        gridLayout_2->addLayout(verticalLayout_3, 5, 5, 1, 1);

        label_2 = new QLabel(verticalLayoutWidget);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        gridLayout_2->addWidget(label_2, 1, 3, 1, 1);

        label_6 = new QLabel(verticalLayoutWidget);
        label_6->setObjectName(QString::fromUtf8("label_6"));

        gridLayout_2->addWidget(label_6, 2, 3, 1, 1);

        pushButton_2 = new QPushButton(verticalLayoutWidget);
        pushButton_2->setObjectName(QString::fromUtf8("pushButton_2"));

        gridLayout_2->addWidget(pushButton_2, 1, 2, 1, 1);

        label_4 = new QLabel(verticalLayoutWidget);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        label_4->setTextFormat(Qt::PlainText);

        gridLayout_2->addWidget(label_4, 2, 1, 1, 1);

        line = new QFrame(verticalLayoutWidget);
        line->setObjectName(QString::fromUtf8("line"));
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);

        gridLayout_2->addWidget(line, 3, 0, 1, 6);

        pushButton_4 = new QPushButton(verticalLayoutWidget);
        pushButton_4->setObjectName(QString::fromUtf8("pushButton_4"));

        gridLayout_2->addWidget(pushButton_4, 2, 5, 1, 1);

        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        scrollArea_2 = new QScrollArea(verticalLayoutWidget);
        scrollArea_2->setObjectName(QString::fromUtf8("scrollArea_2"));
        scrollArea_2->setMinimumSize(QSize(750, 800));
        scrollArea_2->setWidgetResizable(true);
        scrollAreaWidgetContents_2 = new QWidget();
        scrollAreaWidgetContents_2->setObjectName(QString::fromUtf8("scrollAreaWidgetContents_2"));
        scrollAreaWidgetContents_2->setGeometry(QRect(0, 0, 748, 798));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(scrollAreaWidgetContents_2->sizePolicy().hasHeightForWidth());
        scrollAreaWidgetContents_2->setSizePolicy(sizePolicy1);
        scrollArea_2->setWidget(scrollAreaWidgetContents_2);

        verticalLayout_2->addWidget(scrollArea_2);


        gridLayout_2->addLayout(verticalLayout_2, 5, 1, 1, 3);

        pushButton = new QPushButton(verticalLayoutWidget);
        pushButton->setObjectName(QString::fromUtf8("pushButton"));
        sizePolicy.setHeightForWidth(pushButton->sizePolicy().hasHeightForWidth());
        pushButton->setSizePolicy(sizePolicy);
        pushButton->setMinimumSize(QSize(402, 0));

        gridLayout_2->addWidget(pushButton, 1, 0, 1, 1);

        label_5 = new QLabel(verticalLayoutWidget);
        label_5->setObjectName(QString::fromUtf8("label_5"));

        gridLayout_2->addWidget(label_5, 4, 5, 1, 1);

        label_3 = new QLabel(verticalLayoutWidget);
        label_3->setObjectName(QString::fromUtf8("label_3"));

        gridLayout_2->addWidget(label_3, 4, 1, 1, 2);

        label = new QLabel(verticalLayoutWidget);
        label->setObjectName(QString::fromUtf8("label"));

        gridLayout_2->addWidget(label, 1, 1, 1, 1);

        verticalLayout_4 = new QVBoxLayout();
        verticalLayout_4->setObjectName(QString::fromUtf8("verticalLayout_4"));
        scrollArea = new QScrollArea(verticalLayoutWidget);
        scrollArea->setObjectName(QString::fromUtf8("scrollArea"));
        sizePolicy1.setHeightForWidth(scrollArea->sizePolicy().hasHeightForWidth());
        scrollArea->setSizePolicy(sizePolicy1);
        scrollArea->setMinimumSize(QSize(400, 0));
        scrollArea->setFrameShape(QFrame::Box);
        scrollArea->setFrameShadow(QFrame::Sunken);
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName(QString::fromUtf8("scrollAreaWidgetContents"));
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 451, 814));
        sizePolicy1.setHeightForWidth(scrollAreaWidgetContents->sizePolicy().hasHeightForWidth());
        scrollAreaWidgetContents->setSizePolicy(sizePolicy1);
        scrollArea->setWidget(scrollAreaWidgetContents);

        verticalLayout_4->addWidget(scrollArea);


        gridLayout_2->addLayout(verticalLayout_4, 4, 0, 2, 1);

        pushButton_5 = new QPushButton(verticalLayoutWidget);
        pushButton_5->setObjectName(QString::fromUtf8("pushButton_5"));

        gridLayout_2->addWidget(pushButton_5, 2, 2, 1, 1);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_2->addItem(horizontalSpacer_2, 1, 5, 1, 1);

        line_2 = new QFrame(verticalLayoutWidget);
        line_2->setObjectName(QString::fromUtf8("line_2"));
        line_2->setLineWidth(1);
        line_2->setFrameShape(QFrame::VLine);
        line_2->setFrameShadow(QFrame::Sunken);

        gridLayout_2->addWidget(line_2, 1, 4, 2, 1);


        verticalLayout->addLayout(gridLayout_2);


        retranslateUi(Classify);

        QMetaObject::connectSlotsByName(Classify);
    } // setupUi

    void retranslateUi(QWidget *Classify)
    {
        Classify->setWindowTitle(QCoreApplication::translate("Classify", "Form", nullptr));
        pushButton_3->setText(QCoreApplication::translate("Classify", "\351\200\211\346\213\251\350\246\201\345\210\206\347\261\273\347\232\204\345\233\276\347\211\207", nullptr));
        label_2->setText(QString());
        label_6->setText(QCoreApplication::translate("Classify", "TextLabel", nullptr));
        pushButton_2->setText(QCoreApplication::translate("Classify", "\351\200\211\346\213\251\347\275\221\347\273\234\345\256\232\344\271\211\346\226\207\344\273\266", nullptr));
        label_4->setText(QCoreApplication::translate("Classify", "TextLabel", nullptr));
        pushButton_4->setText(QCoreApplication::translate("Classify", "\345\274\200\345\247\213\351\242\204\346\265\213", nullptr));
        pushButton->setText(QCoreApplication::translate("Classify", "\351\200\211\346\213\251\347\275\221\347\273\234\345\217\202\346\225\260\346\226\207\344\273\266init_net  ", nullptr));
        label_5->setText(QCoreApplication::translate("Classify", "\345\210\206\347\261\273\347\273\223\346\236\234", nullptr));
        label_3->setText(QCoreApplication::translate("Classify", "\347\275\221\347\273\234\347\273\223\346\236\204", nullptr));
        label->setText(QString());
        pushButton_5->setText(QCoreApplication::translate("Classify", "\351\200\211\346\213\251\345\210\206\347\261\273\347\273\223\346\236\234\346\226\207\344\273\266", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Classify: public Ui_Classify {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CLASSIFY_H
