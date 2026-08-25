#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QClipboard>
#include <QDirIterator>
#include <QFormLayout>
#include <QToolBar>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QScrollArea>
#include <QSet>
#include <QTabWidget>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

class SimpleBoardWidget : public QWidget
{
public:
    explicit SimpleBoardWidget(QWidget* parent = nullptr): QWidget(parent)
    {
        setMinimumSize(320, 480);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setProperty("activeArea", "");
        setProperty("connected", false);
        setProperty("placementText", tr("Place the card on the reader, then scan."));
    }

    QSize sizeHint() const override
    {
        return QSize(380, 560);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QRectF area = rect().adjusted(10, 10, -10, -10);
        qreal width = qMin(area.width(), area.height() * 0.68);
        QRectF board(area.center().x() - width / 2, area.top(), width, area.height());
        QString active = property("activeArea").toString();
        bool connected = property("connected").toBool();
        bool hfActive = active == "hf";
        bool lfActive = active == "lf";
        QColor boardFill(10, 12, 13);
        QColor boardLine(56, 59, 50);
        QColor white(238, 241, 243);
        QColor muted(188, 198, 205);
        QColor inactiveLine(95, 100, 104);
        QColor inactiveText(120, 126, 132);
        QColor hf = (!connected || lfActive) ? inactiveLine : (hfActive ? QColor(102, 194, 255) : white);
        QColor rfText = (!connected || lfActive) ? inactiveText : white;
        QColor lfLine = (!connected || hfActive) ? inactiveLine : white;
        QColor lfText = (!connected || hfActive) ? inactiveText : white;
        QColor lfFreq = (!connected || hfActive) ? inactiveText : muted;
        QColor red = (!connected || hfActive) ? QColor(92, 96, 100) : (lfActive ? QColor(255, 68, 68) : QColor(245, 34, 34));
        QColor gold(255, 198, 55);

        painter.setPen(QPen(boardLine, 2));
        painter.setBrush(boardFill);
        painter.drawRoundedRect(board, 16, 16);

        painter.setPen(QPen(gold, 2));
        painter.setBrush(gold);
        const qreal screw = 22;
        QList<QPointF> screws = {
            board.topLeft() + QPointF(22, 22),
            board.topRight() + QPointF(-22, 22),
            board.bottomLeft() + QPointF(22, -22),
            board.bottomRight() + QPointF(-22, -22),
        };
        for(const QPointF& point : screws)
        {
            painter.drawEllipse(point, screw / 2, screw / 2);
            painter.setBrush(boardFill);
            painter.drawEllipse(point, screw / 3.6, screw / 3.6);
            painter.setBrush(gold);
        }

        painter.setPen(QPen(gold, 5, Qt::SolidLine, Qt::RoundCap));
        qreal waveSize = board.width() * 0.18;
        painter.drawArc(QRectF(board.left() + 34, board.top() + 30, waveSize, waveSize), 90 * 16, 90 * 16);
        painter.drawArc(QRectF(board.left() + 48, board.top() + 44, waveSize * 0.65, waveSize * 0.65), 90 * 16, 90 * 16);
        painter.drawArc(QRectF(board.right() - 34 - waveSize, board.top() + 30, waveSize, waveSize), 0, 90 * 16);
        painter.drawArc(QRectF(board.right() - 48 - waveSize * 0.65, board.top() + 44, waveSize * 0.65, waveSize * 0.65), 0, 90 * 16);

        QRectF rfCard(board.center().x() - board.width() * 0.22,
                      board.top() + board.height() * 0.16,
                      board.width() * 0.44,
                      board.height() * 0.18);
        painter.setPen(QPen(hf, active == "hf" ? 4 : 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rfCard, 18, 18);

        painter.setPen(QPen(hf, 4, Qt::SolidLine, Qt::RoundCap));
        for(int i = 0; i < 3; i++)
        {
            qreal grow = i * 12;
            painter.drawArc(QRectF(rfCard.center().x() - 58 - grow / 2, rfCard.top() - 58 - grow / 2, 116 + grow, 54 + grow), 30 * 16, 120 * 16);
            painter.drawArc(QRectF(rfCard.left() - 76 - grow, rfCard.center().y() - 48 - grow / 2, 76 + grow, 96 + grow), -55 * 16, 110 * 16);
            painter.drawArc(QRectF(rfCard.right(), rfCard.center().y() - 48 - grow / 2, 76 + grow, 96 + grow), 125 * 16, 110 * 16);
        }

        QFont rfFont = font();
        rfFont.setBold(true);
        rfFont.setPointSize(rfFont.pointSize() + 10);
        painter.setFont(rfFont);
        painter.setPen(rfText);
        painter.drawText(rfCard.adjusted(0, 8, 0, -rfCard.height() * 0.40), Qt::AlignCenter, "RF");

        QFont rfFreqFont = font();
        rfFreqFont.setBold(true);
        rfFreqFont.setPointSize(rfFreqFont.pointSize() + 3);
        painter.setFont(rfFreqFont);
        painter.setPen(rfText);
        painter.drawText(rfCard.adjusted(0, rfCard.height() * 0.42, 0, -10), Qt::AlignCenter, "13.56MHz");

        QRectF lfPanel(board.left() + board.width() * 0.11,
                       board.top() + board.height() * 0.50,
                       board.width() * 0.78,
                       board.height() * 0.43);

        QRectF placementRect(board.left() + board.width() * 0.10,
                             rfCard.bottom() + 18,
                             board.width() * 0.80,
                             qMax<qreal>(34, lfPanel.top() - rfCard.bottom() - 36));
        QFont placementFont = font();
        placementFont.setBold(false);
        placementFont.setPointSize(qMax(8, placementFont.pointSize()));
        painter.setFont(placementFont);
        painter.setPen(connected ? QColor(255, 204, 128) : inactiveText);
        painter.drawText(placementRect, Qt::AlignCenter | Qt::TextWordWrap, property("placementText").toString());

        qreal bevel = 26;
        QPolygonF lfOutline;
        lfOutline << QPointF(lfPanel.left() + bevel, lfPanel.top())
                  << QPointF(lfPanel.right() - bevel, lfPanel.top())
                  << QPointF(lfPanel.right(), lfPanel.top() + bevel)
                  << QPointF(lfPanel.right(), lfPanel.bottom() - bevel)
                  << QPointF(lfPanel.right() - bevel, lfPanel.bottom())
                  << QPointF(lfPanel.left() + bevel, lfPanel.bottom())
                  << QPointF(lfPanel.left(), lfPanel.bottom() - bevel)
                  << QPointF(lfPanel.left(), lfPanel.top() + bevel);
        painter.setPen(QPen(lfLine, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolygon(lfOutline);

        QPointF coilCenter(lfPanel.center().x(), lfPanel.center().y() + 5);
        qreal coilRadius = qMin(lfPanel.width(), lfPanel.height()) * 0.37;
        painter.setPen(QPen(lfLine, 2));
        painter.drawEllipse(coilCenter, coilRadius + 5, coilRadius + 5);
        painter.drawEllipse(coilCenter, coilRadius - 5, coilRadius - 5);
        painter.setPen(QPen(red, active == "lf" ? 18 : 15));
        painter.drawEllipse(coilCenter, coilRadius - 2, coilRadius - 2);

        QFont lfFont = font();
        lfFont.setBold(true);
        lfFont.setPointSize(lfFont.pointSize() + 10);
        painter.setFont(lfFont);
        painter.setPen(lfText);
        painter.drawText(QRectF(coilCenter.x() - 80, coilCenter.y() - 46, 160, 38), Qt::AlignCenter, "LF");

        QFont lfFreqFont = font();
        lfFreqFont.setBold(true);
        lfFreqFont.setPointSize(lfFreqFont.pointSize() + 2);
        painter.setFont(lfFreqFont);
        painter.setPen(lfFreq);
        painter.drawText(QRectF(coilCenter.x() - 105, coilCenter.y() + 4, 210, 30), Qt::AlignCenter, "125 / 134KHz");
    }
};

MainWindow::MainWindow(QWidget *parent):
    QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    dockAllWindows = new QAction(tr("Dock all windows"), this);
    myInfo = new QAction("Proxmark3GUI Modern", this);
    currVersion = new QAction(tr("Ver: ") + QApplication::applicationVersion().section('.', 0, -2), this); // ignore the 4th version number
    checkUpdate = new QAction(tr("Check Update"), this);
    connect(dockAllWindows, &QAction::triggered, [ = ]()
    {
        for(int i = 0; i < dockList.size(); i++)
            dockList[i]->setFloating(false);
    });
    connect(myInfo, &QAction::triggered, [ = ]()
    {
        QDesktopServices::openUrl(QUrl("https://github.com/unicastbg/Proxmark3GUI-Modern"));
    });
    connect(checkUpdate, &QAction::triggered, [ = ]()
    {
        QDesktopServices::openUrl(QUrl("https://github.com/unicastbg/Proxmark3GUI-Modern/releases"));
    });

    settings = new QSettings("GUIsettings.ini", QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings->setIniCodec("UTF-8");
#endif

    pm3Thread = new QThread(this);
    connect(QApplication::instance(), &QApplication::aboutToQuit, pm3Thread, &QThread::quit);
    pm3 = new PM3Process(pm3Thread);
    connect(pm3Thread, &QThread::finished, pm3, &PM3Process::deleteLater);
    pm3Thread->start();
    pm3state = false;
    clientWorkingDir = new QDir;

    util = new Util(this);
    Util::setUI(ui);
    mifare = new Mifare(ui, util, this);
    lf = new LF(ui, util, this);
    t55xxTab = new T55xxTab(util);
    connect(lf, &LF::LFfreqConfChanged, this, &MainWindow::onLFfreqConfChanged);
    connect(t55xxTab, &T55xxTab::setParentGUIState, this, &MainWindow::setState);
    ui->funcTab->insertTab(2, t55xxTab, tr("T55xx"));

    keyEventFilter = new MyEventFilter(QEvent::KeyPress);
    resizeEventFilter = new MyEventFilter(QEvent::Resize);

    // hide unused tabs
//    ui->funcTab->removeTab(1);
    ui->funcTab->removeTab(3);

    portSearchTimer = new QTimer(this);
    portSearchTimer->setInterval(2000);
    connect(portSearchTimer, &QTimer::timeout, this, &MainWindow::on_portSearchTimer_timeout);
    portSearchTimer->start();

    contextMenu = new QMenu();
    contextMenu->addAction(dockAllWindows);

}

MainWindow::~MainWindow()
{
    delete ui;
    emit killPM3();
    pm3Thread->exit(0);
    pm3Thread->wait(5000);
    delete pm3;
    delete pm3Thread;
}

void MainWindow::loadConfig()
{
    QString filename = ui->Set_Client_configFileBox->currentData().toString();
    if(filename == "(ext)")
        filename = ui->Set_Client_configPathEdit->text();
    qDebug() << "config file:" << filename;
    QFile configList(filename);
    if(!configList.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox::information(this, tr("Info"), tr("Failed to load config file"));
        return;
    }

    QByteArray configData = configList.readAll();
    QJsonDocument configJson(QJsonDocument::fromJson(configData));
    mifare->setConfigMap(configJson.object()["mifare classic"].toObject().toVariantMap());
    lf->setConfigMap(configJson.object()["lf"].toObject().toVariantMap());
    t55xxTab->setConfigMap(configJson.object()["t55xx"].toObject().toVariantMap());
}

void MainWindow::initUI() // will be called by main.app
{
    ui->retranslateUi(this);
    uiInit();
    signalInit();
    setState(false);
    dockInit();
}

// ******************** basic functions ********************

void MainWindow::on_portSearchTimer_timeout()
{
    QStringList newPortList; // for actural port name
    QStringList newPortNameList; // for display name
    QStringList registryPorts;
    QStringList registryHintPorts;
    QStringList dosPorts;
    QStringList dosHintPorts;
    const QString hint = " *";

#ifdef Q_OS_WIN
    QSettings serialMap("HKEY_LOCAL_MACHINE\\HARDWARE\\DEVICEMAP\\SERIALCOMM", QSettings::NativeFormat);
    for(const QString& key : serialMap.allKeys())
    {
        QString portName = serialMap.value(key).toString().trimmed();
        if(portName.isEmpty() || registryPorts.contains(portName))
            continue;

        registryPorts << portName;
        QString deviceName = key.toLower();
        if(deviceName.contains("usbser") || deviceName.contains("proxmark") || deviceName.contains("iceman"))
            registryHintPorts << portName;
    }

    for(int i = 1; i <= 256; i++)
    {
        QString portName = QString("COM%1").arg(i);
        WCHAR target[1024] = {};
        DWORD length = QueryDosDeviceW(reinterpret_cast<LPCWSTR>(portName.utf16()), target, 1024);
        if(length == 0 || dosPorts.contains(portName))
            continue;

        dosPorts << portName;
        QString targetName = QString::fromWCharArray(target).toLower();
        if(targetName.contains("usbser") || targetName.contains("proxmark") || targetName.contains("iceman"))
            dosHintPorts << portName;
    }
#endif

    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
//        qDebug() << info.isNull() << info.portName() << info.description() << info.serialNumber() << info.manufacturer();
        if(!info.isNull())
        {
            QString idString = (info.description() + info.serialNumber() + info.manufacturer()).toLower();
            QString portName = info.portName();
            bool preferPort = registryHintPorts.contains(portName) || dosHintPorts.contains(portName);

            newPortList << portName;
            if(info.hasVendorIdentifier() && info.hasProductIdentifier())
            {
                quint16 vid = info.vendorIdentifier();
                quint16 pid = info.productIdentifier();
                if(vid == 0x9AC4 && pid == 0x4B8F)
                    preferPort = true;
                else if(vid == 0x2D2D && pid == 0x504D)
                    preferPort = true;
            }
            else if(idString.contains("proxmark") || idString.contains("iceman"))
                preferPort = true;

            if(preferPort)
                portName += hint;
            newPortNameList << portName;
        }
    }

#ifdef Q_OS_WIN
    for(const QString& registryPort : qAsConst(registryPorts))
    {
        if(newPortList.contains(registryPort))
            continue;

        QString displayName = registryPort;
        if(registryHintPorts.contains(registryPort))
            displayName += hint;
        newPortList << registryPort;
        newPortNameList << displayName;
    }

    for(const QString& dosPort : qAsConst(dosPorts))
    {
        if(newPortList.contains(dosPort))
            continue;

        QString displayName = dosPort;
        if(dosHintPorts.contains(dosPort))
            displayName += hint;
        newPortList << dosPort;
        newPortNameList << displayName;
    }
#endif

    QString selectedPort = ui->PM3_portBox->currentData().toString();
    if(selectedPort.isEmpty())
        selectedPort = ui->PM3_portBox->currentText().remove(hint).trimmed();

    portList = newPortList;
    ui->PM3_portBox->clear();
    int selectId = -1;
    int previousSelectId = -1;
    for(int i = 0; i < portList.size(); i++)
    {
        ui->PM3_portBox->addItem(newPortNameList[i], newPortList[i]);
        if(selectId == -1 && newPortNameList[i].endsWith(hint))
            selectId = i;
        if(previousSelectId == -1 && newPortList[i] == selectedPort)
            previousSelectId = i;
    }
    if(selectId != -1)
        ui->PM3_portBox->setCurrentIndex(selectId);
    else if(previousSelectId != -1)
        ui->PM3_portBox->setCurrentIndex(previousSelectId);
    simpleSyncPorts();
}

void MainWindow::on_PM3_connectButton_clicked()
{
    qDebug() << "Main:" << QThread::currentThread();

    const QComboBox* portBox = ui->PM3_portBox;
    QString port;
    if(portBox->currentText() == portBox->itemText(portBox->currentIndex()))
        // in the list
        port = portBox->currentData().toString();
    else
        // not in the list
        port = portBox->currentText();
    qDebug() << "port:" << port;
    QString startArgs = ui->Set_Client_startArgsEdit->text();
    QString clientPath = ui->PM3_pathBox->currentText();
    QFileInfo clientFile(clientPath);
    bool clientExist = false;

    QStringList extList = {""};
#ifdef Q_OS_WIN
    if(clientFile.suffix().isEmpty())
    {
        QString pathExt = QProcessEnvironment::systemEnvironment().value("pathext");
        extList += pathExt.split(";", Qt::SkipEmptyParts);
        if(extList.size() == 1)
            extList += ".exe";
    }
#endif
    for(const QString& ext : extList)
    {
        QFileInfo executable(clientFile.filePath() + ext);
        if(executable.isFile())
        {
            clientExist = true;
            break;
        }
    }

    if(!clientExist)
    {
        QMessageBox::information(this, tr("Info"), tr("The client path is invalid"), QMessageBox::Ok);
        return;
    }

    // on RRG repo, if no port is specified, the client will search the available port
    if(port == "" && startArgs.contains("<port>")) // has <port>, no port
    {
        QMessageBox::information(this, tr("Info"), tr("Plz choose a port first"), QMessageBox::Ok);
        return;
    }

    if(!startArgs.contains("<port>")) // no <port>
        port = ""; // a symbol

    QStringList args = startArgs.replace("<port>", port).split(' ');
    addClientPath(clientPath);

    QString envScriptPath = ui->Set_Client_envScriptEdit->text();
    if(envScriptPath.contains("<client dir>"))
        envScriptPath.replace("<client dir>", clientFile.absoluteDir().absolutePath());

    QFileInfo envScript(envScriptPath);
    if(envScript.exists())
    {
        qDebug() << envScript.absoluteFilePath();
#ifdef Q_OS_WIN
        QString clientDirPath = QDir::toNativeSeparators(clientFile.absoluteDir().absolutePath());
        QString clientLibsPath = QDir::toNativeSeparators(clientFile.absoluteDir().absoluteFilePath("libs"));
        QString clientShellPath = QDir::toNativeSeparators(clientFile.absoluteDir().absoluteFilePath("libs/shell"));
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString pathValue = env.value("PATH");
        if(pathValue.isEmpty())
            pathValue = env.value("Path");
        env.insert("HOME", clientDirPath + "\\");
        env.insert("QT_PLUGIN_PATH", clientLibsPath + "\\");
        env.insert("QT_QPA_PLATFORM_PLUGIN_PATH", clientLibsPath + "\\");
        env.insert("PATH", clientLibsPath + ";" + clientShellPath + ";" + pathValue);
        env.insert("Path", clientLibsPath + ";" + clientShellPath + ";" + pathValue);
        env.insert("MSYSTEM", "MINGW64");
        clientEnv = env.toStringList();
        emit setProcEnv(&clientEnv);
#else
        QProcess envSetProcess;
        // use the shell session to keep the environment then read it
        // need implementation(or test if space works)
        // sh -c '. "<path>">>/dev/null && env'
        envSetProcess.start("sh -c \' . \"" + envScript.absoluteFilePath() + "\">>/dev/null && env");
        envSetProcess.waitForReadyRead(10000);
        QString envSetResult = QString(envSetProcess.readAll());
#if (QT_VERSION <= QT_VERSION_CHECK(5,14,0))
        clientEnv = envSetResult.split("\n", QString::SkipEmptyParts);
#else
        clientEnv = envSetResult.split("\n", Qt::SkipEmptyParts);
#endif
        if(clientEnv.size() > 2) // the first element is "set" and the last element is the current path
        {
            clientEnv.removeFirst();
            clientEnv.removeLast();
            emit setProcEnv(&clientEnv);
        }
//      qDebug() << "Get Env List" << clientEnv;
        envSetProcess.kill();
#endif
    }
    else
        clientEnv.clear();

    QString workingDirPath = ui->Set_Client_workingDirEdit->text();
    if(workingDirPath.contains("<client dir>"))
        workingDirPath.replace("<client dir>", clientFile.absoluteDir().absolutePath());
    if(workingDirPath.isEmpty())
        workingDirPath = QApplication::applicationDirPath();

    QFileInfo workingDirInfo(workingDirPath);
    if(workingDirInfo.isAbsolute())
    {
        clientWorkingDir->setPath(workingDirPath);
        clientWorkingDir->mkpath(".");
    }
    else
    {
        clientWorkingDir->setPath(QApplication::applicationDirPath());
        clientWorkingDir->mkpath(workingDirPath);
        clientWorkingDir->cd(workingDirPath);
    }
    qDebug() << clientWorkingDir->absolutePath();
    emit setWorkingDir(clientWorkingDir->absolutePath());

    loadConfig();
    emit connectPM3(clientPath, args);
    if(port != "" && !keepClientActive)
        emit setSerialListener(port, true);
    else if(!keepClientActive)
        emit setSerialListener(false);
}

void MainWindow::onPM3ErrorOccurred(QProcess::ProcessError error)
{
    qDebug() << "PM3 Error:" << error << pm3->errorString();
    if(error == QProcess::FailedToStart)
        QMessageBox::information(this, tr("Info"), tr("Failed to start the client") + "\n" + pm3->errorString());
}

void MainWindow::onPM3HWConnectFailed()
{
    QMessageBox::information(this, tr("Info"), tr("Failed to connect to the hardware"));
}

void MainWindow::onPM3StateChanged(bool st, const QString& info)
{
    pm3state = st;
    setState(st);
    if(st == true)
    {
        portSearchTimer->stop();
        setStatusBar(PM3VersionBar, info);
        setStatusBar(connectStatusBar, tr("Connected"));
        stopButton->setEnabled(true);
        ui->PM3_disconnectButton->setEnabled(true);
        if(simpleDisconnectButton != nullptr)
            simpleDisconnectButton->setEnabled(true);
        if(simpleConnectionLabel != nullptr)
        {
            simpleConnectionLabel->setText(tr("Connected"));
            simpleConnectionLabel->setStyleSheet("color: #52d273; font-weight: 700;");
        }
        QString initialAdvice = simpleFirmwareAdvice(info);
        if(initialAdvice.startsWith("OK:"))
            simpleSetFirmwareStatus(initialAdvice.mid(3).trimmed(), "green");
        else
            simpleSetFirmwareStatus(tr("Checking firmware compatibility..."), "");
        QTimer::singleShot(500, this, &MainWindow::simpleCheckFirmwareHealth);
        if(simpleBoardWidget != nullptr)
        {
            simpleBoardWidget->setProperty("connected", true);
            simpleBoardWidget->setProperty("activeArea", "");
            simpleBoardWidget->update();
        }
    }
    else
    {
        portSearchTimer->start();
        setStatusBar(PM3VersionBar, "");
        setStatusBar(connectStatusBar, tr("Not Connected"));
        stopButton->setEnabled(false);
        ui->PM3_disconnectButton->setEnabled(false);
        if(simpleDisconnectButton != nullptr)
            simpleDisconnectButton->setEnabled(false);
        if(simpleConnectionLabel != nullptr)
        {
            simpleConnectionLabel->setText(tr("Not connected"));
            simpleConnectionLabel->setStyleSheet("");
        }
        simpleSetFirmwareStatus(tr("Firmware check will run after connect."), "");
        if(simpleBoardWidget != nullptr)
        {
            simpleBoardWidget->setProperty("connected", false);
            simpleBoardWidget->setProperty("activeArea", "");
            simpleBoardWidget->update();
        }
    }
}

void MainWindow::on_PM3_disconnectButton_clicked()
{
    emit killPM3();
    emit setSerialListener(false);
}

void MainWindow::refreshOutput(const QString& output)
{
//    qDebug() << "MainWindow::refresh:" << output;
    ui->Raw_outputEdit->moveCursor(QTextCursor::End);
    ui->Raw_outputEdit->insertPlainText(output);
    ui->Raw_outputEdit->moveCursor(QTextCursor::End);
}

void MainWindow::on_stopButton_clicked()
{
    simpleSetStatus(tr("Stopping the current client session. Connect again when it finishes."), "amber");
    on_PM3_disconnectButton_clicked();
}
// *********************************************************

// ******************** raw command ********************

void MainWindow::on_Raw_CMDEdit_textChanged(const QString &arg1)
{
    stashedCMDEditText = arg1;
}

void MainWindow::on_Raw_sendCMDButton_clicked()
{
    util->execCMD(ui->Raw_CMDEdit->text());
    refreshCMD(ui->Raw_CMDEdit->text());
}

void MainWindow::on_Raw_clearOutputButton_clicked()
{
    ui->Raw_outputEdit->clear();
}

void MainWindow::on_Raw_CMDHistoryBox_stateChanged(int arg1)
{
    if(arg1 == Qt::Checked)
    {
        ui->Raw_CMDHistoryWidget->setVisible(true);
        ui->Raw_clearHistoryButton->setVisible(true);
        ui->Raw_CMDHistoryBox->setText(tr("History:"));
    }
    else
    {
        ui->Raw_CMDHistoryWidget->setVisible(false);
        ui->Raw_clearHistoryButton->setVisible(false);
        ui->Raw_CMDHistoryBox->setText("");
    }
}

void MainWindow::on_Raw_clearHistoryButton_clicked()
{
    ui->Raw_CMDHistoryWidget->clear();
}

void MainWindow::on_Raw_CMDHistoryWidget_itemDoubleClicked(QListWidgetItem *item)
{
    ui->Raw_CMDEdit->setText(item->text());
    ui->Raw_CMDEdit->setFocus();
}

void MainWindow::sendMSG() // send command when pressing Enter
{
    if(ui->Raw_CMDEdit->hasFocus())
        on_Raw_sendCMDButton_clicked();
}


void MainWindow::refreshCMD(const QString& cmd)
{
    ui->Raw_CMDEdit->blockSignals(true);
    ui->Raw_CMDEdit->setText(cmd);
    if(cmd != "" && (ui->Raw_CMDHistoryWidget->count() == 0 || ui->Raw_CMDHistoryWidget->item(ui->Raw_CMDHistoryWidget->count() - 1)->text() != cmd))
        ui->Raw_CMDHistoryWidget->addItem(cmd);
    stashedCMDEditText = cmd;
    stashedIndex = -1;
    ui->Raw_CMDEdit->blockSignals(false);
}

void MainWindow::on_Raw_keyPressed(QObject* obj_addr, QEvent& event)
{
    if(event.type() == QEvent::KeyPress)
    {
        QKeyEvent& keyEvent = static_cast<QKeyEvent&>(event);
        if(obj_addr == ui->Raw_CMDEdit)
        {
            if(keyEvent.key() == Qt::Key_Up)
            {
                if(stashedIndex > 0)
                    stashedIndex--;
                else if(stashedIndex == -1)
                    stashedIndex = ui->Raw_CMDHistoryWidget->count() - 1;
            }
            else if(keyEvent.key() == Qt::Key_Down)
            {
                if(stashedIndex < ui->Raw_CMDHistoryWidget->count() - 1 && stashedIndex != -1)
                    stashedIndex++;
                else if(stashedIndex == ui->Raw_CMDHistoryWidget->count() - 1)
                    stashedIndex = -1;
            }
            if(keyEvent.key() == Qt::Key_Up || keyEvent.key() == Qt::Key_Down)
            {
                ui->Raw_CMDEdit->blockSignals(true);
                if(stashedIndex == -1)
                    ui->Raw_CMDEdit->setText(stashedCMDEditText);
                else
                    ui->Raw_CMDEdit->setText(ui->Raw_CMDHistoryWidget->item(stashedIndex)->text());
                ui->Raw_CMDEdit->blockSignals(false);
            }
        }
        else if(obj_addr == ui->Raw_outputEdit)
        {
            if(keyEvent.key() == Qt::Key_Up || keyEvent.key() == Qt::Key_Down)
                ui->Raw_CMDEdit->setFocus();
        }
    }
}
// *****************************************************

// ******************** mifare ********************
void MainWindow::on_MF_keyWidget_resized(QObject* obj_addr, QEvent& event)
{
    if(obj_addr == ui->MF_keyWidget && event.type() == QEvent::Resize)
    {
        QTableWidget* widget = (QTableWidget*)obj_addr;
        int keyItemWidth = widget->width();
        keyItemWidth -= widget->verticalScrollBar()->width();
        keyItemWidth -= 2 * widget->frameWidth();
        keyItemWidth -= widget->horizontalHeader()->sectionSize(0);
        widget->horizontalHeader()->resizeSection(1, keyItemWidth / 2);
        widget->horizontalHeader()->resizeSection(2, keyItemWidth / 2);
    }
}

void MainWindow::MF_onMFCardTypeChanged(int id, bool st)
{
    MFCardTypeBtnGroup->blockSignals(true);
    qDebug() << id << MFCardTypeBtnGroup->checkedId();
    if(!st)
    {
        int result;
        if(id > MFCardTypeBtnGroup->checkedId()) // id is specified in uiInit() with a proper order, so I can compare the size by id.
        {
            result = QMessageBox::question(this, tr("Info"), tr("Some of the data and key will be cleared.") + "\n" + tr("Continue?"), QMessageBox::Yes | QMessageBox::No);
        }
        else
        {
            result = QMessageBox::Yes;
        }
        if(result == QMessageBox::Yes)
        {
            qDebug() << "Yes";
            mifare->setCardType(MFCardTypeBtnGroup->checkedId());
            MF_widgetReset();
            mifare->data_syncWithDataWidget();
            mifare->data_syncWithKeyWidget();
        }
        else
        {
            qDebug() << "No";
            MFCardTypeBtnGroup->button(id)->setChecked(true);
        }
    }
    MFCardTypeBtnGroup->blockSignals(false);
}

void MainWindow::on_MF_selectAllBox_stateChanged(int arg1)
{
    ui->MF_dataWidget->blockSignals(true);
    ui->MF_selectAllBox->blockSignals(true);
    ui->MF_selectTrailerBox->blockSignals(true);
    if(arg1 == Qt::PartiallyChecked)
    {
        ui->MF_selectAllBox->setTristate(false);
        ui->MF_selectAllBox->setCheckState(Qt::Checked);
    }
    for(int i = 0; i < mifare->cardType.block_size; i++)
    {
        ui->MF_dataWidget->item(i, 1)->setCheckState(ui->MF_selectAllBox->checkState());
    }
    for(int i = 0; i < mifare->cardType.sector_size; i++)
    {
        ui->MF_dataWidget->item(mifare->cardType.blks[i], 0)->setCheckState(ui->MF_selectAllBox->checkState());
    }
    ui->MF_selectTrailerBox->setCheckState(ui->MF_selectAllBox->checkState());
    ui->MF_dataWidget->blockSignals(false);
    ui->MF_selectAllBox->blockSignals(false);
    ui->MF_selectTrailerBox->blockSignals(false);
}


void MainWindow::on_MF_selectTrailerBox_stateChanged(int arg1)
{
    int selectedSubBlocks = 0;

    ui->MF_dataWidget->blockSignals(true);
    ui->MF_selectAllBox->blockSignals(true);
    ui->MF_selectTrailerBox->blockSignals(true);
    if(arg1 == Qt::PartiallyChecked)
    {
        ui->MF_selectTrailerBox->setTristate(false);
        ui->MF_selectTrailerBox->setCheckState(Qt::Checked);
    }
    for(int i = 0; i < mifare->cardType.sector_size; i++)
    {
        ui->MF_dataWidget->item(mifare->cardType.blks[i] + mifare->cardType.blk[i] - 1, 1)->setCheckState(ui->MF_selectTrailerBox->checkState());
        selectedSubBlocks = 0;
        for(int j = 0; j < mifare->cardType.blk[i]; j++)
        {
            if(ui->MF_dataWidget->item(j + mifare->cardType.blks[i], 1)->checkState() == Qt::Checked)
                selectedSubBlocks++;
        }
        if(selectedSubBlocks == 0)
        {
            ui->MF_dataWidget->item(mifare->cardType.blks[i], 0)->setCheckState(Qt::Unchecked);
        }
        else if(selectedSubBlocks == mifare->cardType.blk[i])
        {
            ui->MF_dataWidget->item(mifare->cardType.blks[i], 0)->setCheckState(Qt::Checked);
        }
        else
        {
            ui->MF_dataWidget->item(mifare->cardType.blks[i], 0)->setCheckState(Qt::PartiallyChecked);
        }
    }

    ui->MF_dataWidget->blockSignals(false);
    ui->MF_selectAllBox->blockSignals(false);
    ui->MF_selectTrailerBox->blockSignals(false);
}


void MainWindow::on_MF_data2KeyButton_clicked()
{
    mifare->data_data2Key();
}

void MainWindow::on_MF_key2DataButton_clicked()
{
    mifare->data_key2Data();
}

void MainWindow::on_MF_fillKeysButton_clicked()
{
    mifare->data_fillKeys();
}

void MainWindow::on_MF_trailerDecoderButton_clicked()
{
    decDialog = new MF_trailerDecoderDialog(this);
    decDialog->show();
}

void MainWindow::on_MF_dataWidget_itemChanged(QTableWidgetItem *item)
{
    ui->MF_dataWidget->blockSignals(true);
    ui->MF_selectAllBox->blockSignals(true);
    ui->MF_selectTrailerBox->blockSignals(true);
    if(item->column() == 0)
    {
        int selectedSectors = 0;
        for(int i = 0; i < mifare->cardType.blk[Mifare::data_b2s(item->row())]; i++)
        {
            ui->MF_dataWidget->item(i + item->row(), 1)->setCheckState(item->checkState());
            qDebug() << i << mifare->cardType.blk[item->row()] << i + item->row() << ui->MF_dataWidget->item(i + item->row(), 1)->text();
        }
        for(int i = 0; i < mifare->cardType.sector_size; i++)
        {
            if(ui->MF_dataWidget->item(mifare->cardType.blks[i], 0)->checkState() == Qt::Checked)
            {
                selectedSectors++;
            }
        }
        if(selectedSectors == 0)
        {
            ui->MF_selectAllBox->setCheckState(Qt::Unchecked);
            ui->MF_selectTrailerBox->setCheckState(Qt::Unchecked);
        }
        else if(selectedSectors == mifare->cardType.sector_size)
        {
            ui->MF_selectAllBox->setCheckState(Qt::Checked);
            ui->MF_selectTrailerBox->setCheckState(Qt::Checked);
        }
        else
        {
            ui->MF_selectAllBox->setCheckState(Qt::PartiallyChecked);
            ui->MF_selectTrailerBox->setCheckState(Qt::PartiallyChecked);
        }
    }
    else if(item->column() == 1)
    {
        int selectedSubBlocks = 0;
        int selectedBlocks = 0;
        int selectedTrailers = 0;

        for(int i = 0; i < mifare->cardType.block_size; i++)
        {
            if(ui->MF_dataWidget->item(i, 1)->checkState() == Qt::Checked)
                selectedBlocks++;
        }
        for(int i = 0; i < mifare->cardType.blk[Mifare::data_b2s(item->row())]; i++)
        {
            if(ui->MF_dataWidget->item(i + mifare->cardType.blks[Mifare::data_b2s(item->row())], 1)->checkState() == Qt::Checked)
                selectedSubBlocks++;
        }
        for(int i = 0; i < mifare->cardType.sector_size; i++)
        {
            int targetBlock = mifare->cardType.blks[i] + mifare->cardType.blk[i] - 1;
            if(ui->MF_dataWidget->item(targetBlock, 1)->checkState() == Qt::Checked)
                selectedTrailers++;
        }
        if(selectedBlocks == 0)
        {
            ui->MF_selectAllBox->setCheckState(Qt::Unchecked);
        }
        else if(selectedBlocks == mifare->cardType.block_size)
        {
            ui->MF_selectAllBox->setCheckState(Qt::Checked);
        }
        else
        {
            ui->MF_selectAllBox->setCheckState(Qt::PartiallyChecked);
        }
        if(selectedSubBlocks == 0)
        {
            ui->MF_dataWidget->item(mifare->cardType.blks[Mifare::data_b2s(item->row())], 0)->setCheckState(Qt::Unchecked);
        }
        else if(selectedSubBlocks == mifare->cardType.blk[Mifare::data_b2s(item->row())])
        {
            ui->MF_dataWidget->item(mifare->cardType.blks[Mifare::data_b2s(item->row())], 0)->setCheckState(Qt::Checked);
        }
        else
        {
            ui->MF_dataWidget->item(mifare->cardType.blks[Mifare::data_b2s(item->row())], 0)->setCheckState(Qt::PartiallyChecked);
        }
        if(selectedTrailers == 0)
        {
            ui->MF_selectTrailerBox->setCheckState(Qt::Unchecked);
        }
        else if(selectedTrailers == mifare->cardType.sector_size)
        {
            ui->MF_selectTrailerBox->setCheckState(Qt::Checked);
        }
        else
        {
            ui->MF_selectTrailerBox->setCheckState(Qt::PartiallyChecked);
        }
    }
    else if(item->column() == 2)
    {
        QString data = item->text().remove(" ").toUpper();
        if(data == "" || mifare->data_isDataValid(data) == Mifare::DATA_NOSPACE)
        {
            mifare->data_setData(item->row(), data);
        }
        else
        {
            QMessageBox::information(this, tr("Info"), tr("Data must consists of 32 Hex symbols(Whitespace is allowed)"));
        }
        mifare->data_syncWithDataWidget(false, item->row());
    }
    ui->MF_dataWidget->blockSignals(false);
    ui->MF_selectAllBox->blockSignals(false);
    ui->MF_selectTrailerBox->blockSignals(false);
}

void MainWindow::on_MF_keyWidget_itemChanged(QTableWidgetItem *item)
{
    if(item->column() == 1)
    {
        QString key = item->text().remove(" ").toUpper();
        if(key == "" || mifare->data_isKeyValid(key))
        {
            mifare->data_setKey(item->row(), Mifare::KEY_A, key);
        }
        else
        {
            QMessageBox::information(this, tr("Info"), tr("Key must consists of 12 Hex symbols(Whitespace is allowed)"));
        }
        mifare->data_syncWithKeyWidget(false, item->row(), Mifare::KEY_A);
    }
    else if(item->column() == 2)
    {
        QString key = item->text().remove(" ").toUpper();
        if(key == "" || mifare->data_isKeyValid(key))
        {
            mifare->data_setKey(item->row(), Mifare::KEY_B, key);
        }
        else
        {
            QMessageBox::information(this, tr("Info"), tr("Key must consists of 12 Hex symbols(Whitespace is allowed)"));
        }
        mifare->data_syncWithKeyWidget(false, item->row(), Mifare::KEY_B);
    }
}

void MainWindow::on_MF_File_loadButton_clicked()
{
    QString title = "";
    QString filename = "";
    if(ui->MF_File_dataButton->isChecked())
    {
        title = tr("Plz select the data file:");
        filename = QFileDialog::getOpenFileName(this, title, "./", tr("Binary Data Files(*.bin *.dump)") + ";;" + tr("Text Data Files(*.txt *.eml)") + ";;" + tr("All Files(*.*)"));
        qDebug() << filename;
        if(filename != "")
        {
            if(!mifare->data_loadDataFile(filename))
            {
                QMessageBox::information(this, tr("Info"), tr("Failed to open") + "\n" + filename);
            }
        }
    }
    else if(ui->MF_File_keyButton->isChecked())
    {
        title = tr("Plz select the key file:");
        filename = QFileDialog::getOpenFileName(this, title, "./", tr("Binary Key Files(*.bin *.dump)") + ";;" + tr("All Files(*.*)"));
        qDebug() << filename;
        if(filename != "")
        {
            if(!mifare->data_loadKeyFile(filename))
            {
                QMessageBox::information(this, tr("Info"), tr("Failed to open") + "\n" + filename);
            }
        }
    }

}

void MainWindow::on_MF_File_saveButton_clicked()
{

    QString title = "";
    QString filename = "";
    QString selectedType = "";
    QString defaultName = mifare->data_getUID();
    if(defaultName != "")
        defaultName += "_";
    defaultName += QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss");

    if(ui->MF_File_dataButton->isChecked())
    {
        title = tr("Plz select the location to save data file:");
        filename = QFileDialog::getSaveFileName(this, title, "./data_" + defaultName, tr("Binary Data Files(*.bin *.dump)") + ";;" + tr("Text Data Files(*.txt *.eml)"), &selectedType);
        qDebug() << filename;
        if(filename != "")
        {
            if(!mifare->data_saveDataFile(filename, selectedType == tr("Binary Data Files(*.bin *.dump)")))
            {
                QMessageBox::information(this, tr("Info"), tr("Failed to save to") + "\n" + filename);
            }
        }
    }
    else if(ui->MF_File_keyButton->isChecked())
    {
        title = tr("Plz select the location to save key file:");
        filename = QFileDialog::getSaveFileName(this, title, "./key_" + defaultName, tr("Binary Key Files(*.bin *.dump)"), &selectedType);
        qDebug() << filename;
        if(filename != "")
        {
            if(!mifare->data_saveKeyFile(filename, selectedType == tr("Binary Key Files(*.bin *.dump)")))
            {
                QMessageBox::information(this, tr("Info"), tr("Failed to save to") + "\n" + filename);
            }
        }
    }
    qDebug() << filename << selectedType;
}

void MainWindow::on_MF_File_clearButton_clicked()
{
    if(ui->MF_File_keyButton->isChecked())
    {
        mifare->data_clearKey();
        mifare->data_syncWithKeyWidget();
    }
    else if(ui->MF_File_dataButton->isChecked())
    {
        mifare->data_clearData();
        mifare->data_syncWithDataWidget();
    }
}

void MainWindow::on_MF_Attack_infoButton_clicked()
{
    mifare->info();
}

void MainWindow::on_MF_Attack_chkButton_clicked()
{
    setState(false);
    mifare->chk();
    setState(true);
}

void MainWindow::on_MF_Attack_nestedButton_clicked()
{
    setState(false);
    mifare->nested();
    setState(true);
}

void MainWindow::on_MF_Attack_hardnestedButton_clicked()
{
    mifare->hardnested();
}

void MainWindow::on_MF_RW_readSelectedButton_clicked()
{
    setState(false);
    Util::gotoRawTab();
    mifare->readSelected(Mifare::TARGET_MIFARE);
    setState(true);
}

void MainWindow::on_MF_RW_readBlockButton_clicked()
{
    setState(false);
    Util::gotoRawTab();
    mifare->readOne(Mifare::TARGET_MIFARE);
    setState(true);
}

void MainWindow::on_MF_RW_writeBlockButton_clicked()
{
    setState(false);
    mifare->writeOne();
    setState(true);
}

void MainWindow::on_MF_RW_writeSelectedButton_clicked()
{
    setState(false);
    mifare->writeSelected(Mifare::TARGET_MIFARE);
    setState(true);
}

void MainWindow::on_MF_RW_dumpButton_clicked()
{
    mifare->dump();
}

void MainWindow::on_MF_RW_restoreButton_clicked()
{
    mifare->restore();
}

void MainWindow::on_MF_UID_readSelectedButton_clicked()
{
    setState(false);
    Util::gotoRawTab();
    mifare->readSelected(Mifare::TARGET_UID);
    setState(true);
}

void MainWindow::on_MF_UID_readBlockButton_clicked()
{
    setState(false);
    Util::gotoRawTab();
    mifare->readOne(Mifare::TARGET_UID);
    setState(true);
}

void MainWindow::on_MF_UID_writeSelectedButton_clicked()
{
    setState(false);
    mifare->writeSelected(Mifare::TARGET_UID);
    setState(true);
}

void MainWindow::on_MF_UID_writeBlockButton_clicked()
{
    setState(false);
    mifare->writeOne(Mifare::TARGET_UID);
    setState(true);
}

void MainWindow::on_MF_UID_wipeButton_clicked()
{
    mifare->wipeC();
}

void MainWindow::on_MF_UID_aboutUIDButton_clicked()
{
    QString msg;
    msg += tr("    Normally, the Block 0 of a typical Mifare card, which contains the UID, is locked during the manufacture. Users cannot write anything to Block 0 or set a new UID to a normal Mifare card.") + "\n";
    msg += tr("    Chinese Magic Cards(aka UID Cards) are some special cards whose Block 0 are writeable. And you can change UID by writing to it.") + "\n";
    msg += "\n";
    msg += tr("There are two versions of Chinese Magic Cards, the Gen1 and the Gen2.") + "\n";
    msg += tr("    Gen1:") + "\n" + tr("    also called UID card in China. It responses to some backdoor commands so you can access any blocks without password. The Proxmark3 has a bunch of related commands(csetblk, cgetblk, ...) to deal with this type of card, and my GUI also support these commands.") + "\n";
    msg += tr("    Gen2:") + "\n" + tr("    doesn't response to the backdoor commands, which means that a reader cannot detect whether it is a Chinese Magic Card or not by sending backdoor commands.") + "\n";
    msg += "\n";
    msg += tr("There are some types of Chinese Magic Card Gen2.") + "\n";
    msg += tr("    CUID Card:") + "\n" + tr("    the Block 0 is writeable, you can write to this block repeatedly by normal wrbl command.") + "\n";
    msg += tr("    (hf mf wrbl 0 A FFFFFFFFFFFF <the data you want to write>)") + "\n";
    msg += tr("    FUID Card:") + "\n" + tr("    you can only write to Block 0 once. After that, it seems like a typical Mifare card(Block 0 cannot be written to).") + "\n";
    msg += tr("    (some readers might try changing the Block 0, which could detect the CUID Card. In that case, you should use FUID card.)") + "\n";
    msg += tr("    UFUID Card:") + "\n" + tr("    It behaves like a CUID card(or UID card? I'm not sure) before you send some special command to lock it. Once it is locked, you cannot change its Block 0(just like a typical Mifare card).") + "\n";
    msg += "\n";
    msg += tr("    Seemingly, these Chinese Magic Cards are more easily to be compromised by Nested Attack(it takes little time to get an unknown key).") + "\n";
    QMessageBox::information(this, tr("About UID Card"), msg);
}

void MainWindow::on_MF_UID_setParaButton_clicked()
{
    setState(false);
    mifare->setParameterC();
    setState(true);
}

void MainWindow::on_MF_UID_lockButton_clicked()
{
    mifare->lockC();
}

void MainWindow::on_MF_Sim_readSelectedButton_clicked()
{
    setState(false);
    Util::gotoRawTab();
    mifare->readSelected(Mifare::TARGET_EMULATOR);
    setState(true);
}

void MainWindow::on_MF_Sim_writeSelectedButton_clicked()
{
    setState(false);
    mifare->writeSelected(Mifare::TARGET_EMULATOR);
    setState(true);
}

void MainWindow::on_MF_Sim_clearButton_clicked()
{
    mifare->wipeE();
}

void MainWindow::on_MF_Sim_simButton_clicked()
{
    mifare->simulate();
}

void MainWindow::on_MF_Sniff_loadButton_clicked() // use a tmp file to support complicated path
{
    QString title = "";
    QString filename = "";
    QString defaultExtension;
    QDir clientTracePath;

    if(Util::getClientType() == Util::CLIENTTYPE_OFFICIAL)
        defaultExtension = ".trc";
    else if(Util::getClientType() == Util::CLIENTTYPE_ICEMAN)
        defaultExtension = ".trace";

    QString userTraceSavePath = mifare->getTraceSavePath();
    if(userTraceSavePath.isEmpty())
        clientTracePath = *clientWorkingDir;
    else
        clientTracePath = QDir(userTraceSavePath); // For v4.16717 and later

    title = tr("Plz select the trace file:");
    filename = QFileDialog::getOpenFileName(this, title, clientTracePath.absolutePath(), tr("Trace Files") + "(*" + defaultExtension + ")" + ";;" + tr("All Files(*.*)"));
    qDebug() << filename;
    if(filename != "")
    {
        QString tmpFile = "tmp" + QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch()) + defaultExtension;
        if(QFile::copy(filename, clientTracePath.absolutePath() + "/" + tmpFile))
        {
            mifare->loadSniff(tmpFile);
            util->delay(3000);
            QFile::remove(clientTracePath.absolutePath() + "/" + tmpFile);
        }
        else
        {
            QMessageBox::information(this, tr("Info"), tr("Failed to open") + "\n" + filename);
        }
    }
}

void MainWindow::on_MF_Sniff_saveButton_clicked()
{
    QString title = "";
    QString filename = "";
    QString defaultExtension;
    QDir clientTracePath;

    if(Util::getClientType() == Util::CLIENTTYPE_OFFICIAL)
        defaultExtension = ".trc";
    else if(Util::getClientType() == Util::CLIENTTYPE_ICEMAN)
        defaultExtension = ".trace";

    QString userTraceSavePath = mifare->getTraceSavePath();
    if(userTraceSavePath.isEmpty())
        clientTracePath = *clientWorkingDir;
    else
        clientTracePath = QDir(userTraceSavePath); // For v4.16717 and later

    title = tr("Plz select the location to save trace file:");
    filename = QFileDialog::getSaveFileName(this, title, clientTracePath.absolutePath(), tr("Trace Files") + "(*" + defaultExtension + ")");
    qDebug() << filename;
    if(filename != "")
    {
        QString tmpFile = "tmp" + QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch()) + defaultExtension;
        mifare->saveSniff(tmpFile);
        for(int i = 0; i < 100; i++)
        {
            util->delay(100);
            if(QFile::exists(clientTracePath.absolutePath() + "/" + tmpFile))
                break;
        }
        // filename is not empty -> the user has chosen to overwrite the existing file
        if(QFile::exists(filename))
            QFile::remove(filename);
        if(!QFile::copy(clientTracePath.absolutePath() + "/" + tmpFile, filename))
        {
            QMessageBox::information(this, tr("Info"), tr("Failed to save to") + "\n" + filename);
        }
        QFile::remove(clientTracePath.absolutePath() + "/" + tmpFile);
    }

}

void MainWindow::on_MF_Sniff_sniffButton_clicked()
{
    setState(false);
    mifare->sniff();
    setState(true);
}

void MainWindow::on_MF_14aSniff_snoopButton_clicked()
{
    setState(false);
    mifare->sniff14a();
    setState(true);
}

void MainWindow::on_MF_Sniff_listButton_clicked()
{
    mifare->list();
}

void MainWindow::MF_widgetReset()
{
    int secs = mifare->cardType.sector_size;
    int blks = mifare->cardType.block_size;
    QBrush trailerItemForeColor = QBrush(QColor(0, 160, 255));
    ui->MF_RW_blockBox->clear();
    ui->MF_keyWidget->setRowCount(secs);
    ui->MF_dataWidget->setRowCount(blks);

    ui->MF_dataWidget->blockSignals(true);
    ui->MF_keyWidget->blockSignals(true);
    ui->MF_selectAllBox->blockSignals(true);
    ui->MF_selectTrailerBox->blockSignals(true);

    for(int i = 0; i < blks; i++)
    {
        setTableItem(ui->MF_dataWidget, i, 0, "");
        setTableItem(ui->MF_dataWidget, i, 1, QString::number(i));
        ui->MF_dataWidget->item(i, 1)->setCheckState(Qt::Unchecked);
        setTableItem(ui->MF_dataWidget, i, 2, "");
        ui->MF_RW_blockBox->addItem(QString::number(i));
    }

    for(int i = 0; i < secs; i++)
    {
        setTableItem(ui->MF_keyWidget, i, 0, QString::number(i));
        setTableItem(ui->MF_keyWidget, i, 1, "");
        setTableItem(ui->MF_keyWidget, i, 2, "");
        setTableItem(ui->MF_dataWidget, mifare->cardType.blks[i], 0, QString::number(i));
        ui->MF_dataWidget->item(mifare->cardType.blks[i] + mifare->cardType.blk[i] - 1, 2)->setForeground(trailerItemForeColor);
        ui->MF_dataWidget->item(mifare->cardType.blks[i], 0)->setCheckState(Qt::Unchecked);
    }
    ui->MF_dataWidget->item(0, 2)->setForeground(QBrush(QColor(255, 160, 0)));
    ui->MF_selectAllBox->setCheckState(Qt::Unchecked);
    ui->MF_selectTrailerBox->setCheckState(Qt::Unchecked);

    ui->MF_dataWidget->blockSignals(false);
    ui->MF_keyWidget->blockSignals(false);
    ui->MF_selectAllBox->blockSignals(false);
    ui->MF_selectTrailerBox->blockSignals(false);
}
// ************************************************


// ******************** other ********************

void MainWindow::uiInit()
{
    connect(ui->Raw_CMDEdit, &QLineEdit::returnPressed, this, &MainWindow::sendMSG);
    ui->Raw_CMDEdit->installEventFilter(keyEventFilter);
    connect(keyEventFilter, &MyEventFilter::eventHappened, this, &MainWindow::on_Raw_keyPressed);
    ui->MF_keyWidget->installEventFilter(resizeEventFilter);
    connect(resizeEventFilter, &MyEventFilter::eventHappened, this, &MainWindow::on_MF_keyWidget_resized);
    ui->Raw_outputEdit->installEventFilter(keyEventFilter);

    connectStatusBar = new QLabel(this);
    programStatusBar = new QLabel(this);
    PM3VersionBar = new QLabel(this);
    stopButton = new QPushButton(this);
    setStatusBar(connectStatusBar, tr("Not Connected"));
    setStatusBar(programStatusBar, tr("Idle"));
    setStatusBar(PM3VersionBar, "");
    stopButton->setText(tr("Stop"));
    stopButton->setEnabled(false);
    ui->statusbar->addPermanentWidget(PM3VersionBar, 1);
    ui->statusbar->addPermanentWidget(connectStatusBar, 1);
    ui->statusbar->addPermanentWidget(programStatusBar, 1);
    ui->statusbar->addPermanentWidget(stopButton);

    ui->MF_dataWidget->setColumnWidth(0, 55);
    ui->MF_dataWidget->setColumnWidth(1, 55);

    ui->MF_keyWidget->setColumnWidth(0, 45);

    MF_widgetReset();
    MFCardTypeBtnGroup = new QButtonGroup(this);
    MFCardTypeBtnGroup->addButton(ui->MF_Type_miniButton, 0);
    MFCardTypeBtnGroup->addButton(ui->MF_Type_1kButton, 1);
    MFCardTypeBtnGroup->addButton(ui->MF_Type_2kButton, 2);
    MFCardTypeBtnGroup->addButton(ui->MF_Type_4kButton, 4);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    connect(MFCardTypeBtnGroup, QOverload<int, bool>::of(&QButtonGroup::buttonToggled), this, &MainWindow::MF_onMFCardTypeChanged);
#else
    connect(MFCardTypeBtnGroup, &QButtonGroup::idToggled, this, &MainWindow::MF_onMFCardTypeChanged);
#endif

    ui->MF_keyWidget->installEventFilter(this);
    ui->MF_dataWidget->installEventFilter(this);
    simplePageInit();
    makeTabScrollable(ui->mifareTab);
    makeTabScrollable(ui->lfTab);
    makeTabScrollable(t55xxTab);
    ui->funcTab->setCurrentWidget(simpleTab);

    ui->Set_UI_Theme_nameBox->addItem(tr("Modern Dark"), "modern_dark");
    ui->Set_UI_Theme_nameBox->addItem(tr("(None)"), "(none)");
    ui->Set_UI_Theme_nameBox->addItem(tr("Dark"), "qdss_dark");
    ui->Set_UI_Theme_nameBox->addItem(tr("Light"), "qdss_light");

    settings->beginGroup("UI_grpbox_preference");

    QStringList boxNames = settings->allKeys();
    QGroupBox * boxptr;
    foreach(QString name, boxNames)
    {
        boxptr = this->findChild<QGroupBox*>(name);
        if(boxptr == nullptr)
            continue;
        if(ui->mifareTab->isAncestorOf(boxptr) || ui->lfTab->isAncestorOf(boxptr) || t55xxTab->isAncestorOf(boxptr))
            continue;
        if(settings->value(name, true).toBool())
        {
            boxptr->setMaximumHeight(16777215);
            boxptr->setChecked(true);
        }
        else
        {
            boxptr->setMaximumHeight(20);
            boxptr->setChecked(false);
        }
    }
    settings->endGroup();
    normalizeAdvancedPage(ui->mifareTab);
    normalizeAdvancedPage(ui->lfTab);
    normalizeAdvancedPage(t55xxTab);

    loadClientPathList();

    ui->Set_Client_GUIWorkingDirLabel->setText(QDir::currentPath());

    settings->beginGroup("Client_Args");
    ui->Set_Client_startArgsEdit->setText(settings->value("args", "-p <port> -f").toString());
    settings->endGroup();

    settings->beginGroup("Client_forceButtonsEnabled");
    keepButtonsEnabled = settings->value("state", false).toBool();
    settings->endGroup();
    ui->Set_Client_forceEnabledBox->setChecked(keepButtonsEnabled);

    // the disconnect detection doesn't work well on Linux/macOS
    // So it should be disabled on these platforms
    // https://github.com/wh201906/Proxmark3GUI/issues/22
    // #22, #26, #40, #41
    settings->beginGroup("Client_keepClientActive");
#ifdef Q_OS_WIN
    keepClientActive = settings->value("state", false).toBool();
#else
    keepClientActive = settings->value("state", true).toBool();
#endif
    settings->endGroup();
    ui->Set_Client_keepClientActiveBox->setChecked(keepClientActive);

    QDir configFiles(":/config/");
    configFiles.setSorting(QDir::Name);
    const QFileInfoList configFileList = configFiles.entryInfoList();
    ui->Set_Client_configFileBox->blockSignals(true);
    for(const auto& file : configFileList)
    {
        ui->Set_Client_configFileBox->addItem(file.fileName(), file.filePath());
    }

    // Use the last one as the default one
    ui->Set_Client_configFileBox->setCurrentIndex(ui->Set_Client_configFileBox->count() - 1);
    ui->Set_Client_configFileBox->addItem(tr("External file"), "(ext)");

    int configId = -1;
    settings->beginGroup("Client_Env");
    ui->Set_Client_envScriptEdit->setText(settings->value("scriptPath", "<client dir>/setup.bat").toString());
    ui->Set_Client_workingDirEdit->setText(settings->value("workingDir", "<client dir>").toString());
    configId = ui->Set_Client_configFileBox->findData(settings->value("configFile"));
    ui->Set_Client_configPathEdit->setText(settings->value("extConfigFilePath", "config.json").toString());
    settings->endGroup();
    if(configId != -1)
        ui->Set_Client_configFileBox->setCurrentIndex(configId);
    ui->Set_Client_configFileBox->blockSignals(false);
    on_Set_Client_configFileBox_currentIndexChanged(ui->Set_Client_configFileBox->currentIndex());

    // setValue() will trigger valueChanged()
    // setValue(settings->value()) will create a nested group
    // call endGroup() before apply the value
    settings->beginGroup("UI");
    int themeId = ui->Set_UI_Theme_nameBox->findData(settings->value("Theme_Name", "modern_dark").toString());
    settings->endGroup();
    setWindowOpacity(1.0);
    ui->Set_UI_Opacity_Box->setValue(100);
    ui->Set_UI_Theme_nameBox->setCurrentIndex((themeId == -1) ? 0 : themeId);

    settings->beginGroup("UI");
    // QApplication::font() might return wrong result
    // If fonts are not specified in config file, don't touch them.
    QString tmpFontName;
    int tmpFontSize;
    bool fontValid = false, dataFontValid = false, CMDFontValid = false;
    tmpFontName = settings->value("Font_Name", "").toString();
    tmpFontSize = settings->value("Font_Size", -1).toInt();
    if(!tmpFontName.isEmpty() && tmpFontSize != -1 && tmpFontName == QFont(tmpFontName).family())
    {
        ui->Set_UI_Font_nameBox->setCurrentFont(QFont(tmpFontName));
        ui->Set_UI_Font_sizeBox->setValue(tmpFontSize);
        fontValid = true;
    }
    // The default font should be the same as MF_dataWidget's and MF_keyWidget's.
    tmpFontName = settings->value("DataFont_Name", "Consolas").toString();
    tmpFontSize = settings->value("DataFont_Size", 12).toInt();
    if(!tmpFontName.isEmpty() && tmpFontSize != -1 && tmpFontName == QFont(tmpFontName).family())
    {
        ui->Set_UI_DataFont_nameBox->setCurrentFont(QFont(tmpFontName));
        ui->Set_UI_DataFont_sizeBox->setValue(tmpFontSize);
        dataFontValid = true;
    }
    tmpFontName = settings->value("CMDFont_Name", "").toString();
    tmpFontSize = settings->value("CMDFont_Size", -1).toInt();
    if(!tmpFontName.isEmpty() && tmpFontSize != -1 && tmpFontName == QFont(tmpFontName).family())
    {
        ui->Set_UI_CMDFont_nameBox->setCurrentFont(QFont(tmpFontName));
        ui->Set_UI_CMDFont_sizeBox->setValue(tmpFontSize);
        CMDFontValid = true;
    }
    settings->endGroup();

    if(fontValid)
        on_Set_UI_Font_setButton_clicked();
    if(dataFontValid)
        on_Set_UI_DataFont_setButton_clicked();
    if(CMDFontValid)
        on_Set_UI_CMDFont_setButton_clicked();

    ui->MF_RW_keyTypeBox->addItem("A", Mifare::KEY_A);
    ui->MF_RW_keyTypeBox->addItem("B", Mifare::KEY_B);

    on_Raw_CMDHistoryBox_stateChanged(Qt::Unchecked);

}

void MainWindow::signalInit()
{
    connect(pm3, &PM3Process::newOutput, util, &Util::processOutput);
    connect(pm3, &PM3Process::changeClientType, util, &Util::setClientType);
    connect(util, &Util::refreshOutput, this, &MainWindow::refreshOutput);

    connect(this, &MainWindow::connectPM3, pm3, &PM3Process::connectPM3);
    connect(this, &MainWindow::reconnectPM3, pm3, &PM3Process::reconnectPM3);
    connect(pm3, &PM3Process::PM3StatedChanged, this, &MainWindow::onPM3StateChanged);
    connect(pm3, &PM3Process::PM3StatedChanged, util, &Util::setRunningState);
    connect(pm3, &PM3Process::errorOccurred, this, &MainWindow::onPM3ErrorOccurred);
    connect(pm3, &PM3Process::HWConnectFailed, this, &MainWindow::onPM3HWConnectFailed);
    connect(this, &MainWindow::killPM3, pm3, &PM3Process::killPM3);
    connect(this, &MainWindow::setProcEnv, pm3, &PM3Process::setProcEnv);
    connect(this, &MainWindow::setWorkingDir, pm3, &PM3Process::setWorkingDir);
    connect(this, QOverload<bool>::of(&MainWindow::setSerialListener), pm3, QOverload<bool>::of(&PM3Process::setSerialListener));
    connect(this, QOverload<const QString&, bool>::of(&MainWindow::setSerialListener), pm3, QOverload<const QString&, bool>::of(&PM3Process::setSerialListener));

    connect(util, &Util::write, pm3, &PM3Process::write);

    connect(ui->MF_typeGroupBox, &QGroupBox::clicked, this, &MainWindow::on_GroupBox_clicked);
    connect(ui->MF_fileGroupBox, &QGroupBox::clicked, this, &MainWindow::on_GroupBox_clicked);
    connect(ui->MF_RWGroupBox, &QGroupBox::clicked, this, &MainWindow::on_GroupBox_clicked);
    connect(ui->MF_normalGroupBox, &QGroupBox::clicked, this, &MainWindow::on_GroupBox_clicked);
    connect(ui->MF_UIDGroupBox, &QGroupBox::clicked, this, &MainWindow::on_GroupBox_clicked);
    connect(ui->MF_simGroupBox, &QGroupBox::clicked, this, &MainWindow::on_GroupBox_clicked);
    connect(ui->MF_sniffGroupBox, &QGroupBox::clicked, this, &MainWindow::on_GroupBox_clicked);

    connect(stopButton, &QPushButton::clicked, this, &MainWindow::on_stopButton_clicked);

}

void MainWindow::setStatusBar(QLabel * target, const QString& text)
{
    if(target == PM3VersionBar)
        target->setText(tr("HW Version:") + text);
    else if(target == connectStatusBar)
        target->setText(tr("PM3:") + text);
    else if(target == programStatusBar)
        target->setText(tr("State:") + text);
}

void MainWindow::setTableItem(QTableWidget * widget, int row, int column, const QString& text)
{
    if(widget->item(row, column) == nullptr)
        widget->setItem(row, column, new QTableWidgetItem());
    widget->item(row, column)->setText(text);
}

bool MainWindow::eventFilter(QObject * watched, QEvent * event) // drag support
{
    if(event->type() == QEvent::DragEnter)
    {
        QDragEnterEvent* dragEvent = static_cast<QDragEnterEvent*>(event);
        dragEvent->acceptProposedAction();
        return true;
    }
    else if(event->type() == QEvent::Drop)
    {
        QDropEvent* dropEvent = static_cast<QDropEvent*>(event);
        if(watched == ui->MF_keyWidget)
        {
            const QMimeData* mime = dropEvent->mimeData();
            if(mime->hasUrls())
            {
                QList<QUrl> urls = mime->urls();
                if(urls.length() == 1)
                {
                    mifare->data_loadKeyFile(urls[0].toLocalFile());
                    return true;
                }
            }
        }
        else if(watched == ui->MF_dataWidget)
        {
            const QMimeData* mime = dropEvent->mimeData();
            if(mime->hasUrls())
            {
                QList<QUrl> urls = mime->urls();
                if(urls.length() == 1)
                {
                    mifare->data_loadDataFile(urls[0].toLocalFile());
                    return true;
                }
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setState(bool st)
{
    if(!st && pm3state)
    {
        setStatusBar(programStatusBar, tr("Running"));
    }
    else
    {
        setStatusBar(programStatusBar, tr("Idle"));
    }
    setButtonsEnabled(st || keepButtonsEnabled);
}

void MainWindow::setButtonsEnabled(bool st)
{
    ui->MF_typeGroupBox->setEnabled(st);
    ui->MF_fileGroupBox->setEnabled(st);
    ui->MF_RWGroupBox->setEnabled(st);
    ui->MF_attackGroupBox->setEnabled(st);
    ui->MF_normalGroupBox->setEnabled(st);
    ui->MF_UIDGroupBox->setEnabled(st);
    ui->MF_simGroupBox->setEnabled(st);
    ui->MF_sniffGroupBox->setEnabled(st);
    ui->Raw_CMDEdit->setEnabled(st);
    ui->Raw_sendCMDButton->setEnabled(st);
    ui->LF_LFconfigGroupBox->setEnabled(st);
    ui->LF_operationGroupBox->setEnabled(st);
    if(simpleScanButton != nullptr)
        simpleScanButton->setEnabled(st);
    if(simpleCloneButton != nullptr)
        simpleCloneButton->setEnabled(st && !simpleCloneCommand().isEmpty());
    if(dumpDetectButton != nullptr)
        dumpDetectButton->setEnabled(st);
    if(dumpReadButton != nullptr)
        dumpReadButton->setEnabled(st && !dumpLastFamily.isEmpty() && dumpLastFamily != "Unknown");
    if(dumpWriteButton != nullptr)
        dumpWriteButton->setEnabled(st && !dumpReadOutput.trimmed().isEmpty() && dumpCanWriteFamily(dumpLastFamily));
    if(dumpVerifyButton != nullptr)
        dumpVerifyButton->setEnabled(st && !dumpWriteOutput.trimmed().isEmpty() && dumpCanWriteFamily(dumpLastFamily));
    if(dumpCleanButton != nullptr)
        dumpCleanButton->setEnabled(st && !dumpLastCardId.isEmpty());
}

void MainWindow::on_GroupBox_clicked(bool checked)
{
    QGroupBox* box = dynamic_cast<QGroupBox*>(sender());

    settings->beginGroup("UI_grpbox_preference");
    if(checked)
    {
        box->setMaximumHeight(16777215);
        settings->setValue(box->objectName(), true);
    }
    else
    {
        box->setMaximumHeight(20);
        settings->setValue(box->objectName(), false);
    }
    settings->endGroup();
}

void MainWindow::addClientPath(const QString& path)
{
    m_clientPathList.removeAll(path);
    m_clientPathList.prepend(path);
    while(m_clientPathList.size() > 32) // the maximum count of path items
        m_clientPathList.removeLast();
    // sync to the storage
    saveClientPathList();
    // sync to the UI
    loadClientPathList();
}

QString MainWindow::findModernClientPath() const
{
#ifdef Q_OS_WIN
    const QString clientExe = "proxmark3.exe";
#else
    const QString clientExe = "proxmark3";
#endif

    QStringList roots;
    roots << QApplication::applicationDirPath() << QDir::currentPath();

    for(const QString& root : qAsConst(roots))
    {
        QDir dir(root);
        for(int depth = 0; depth < 8; depth++)
        {
            QString directClient = QDir::cleanPath(dir.filePath(clientExe));
            if(QFileInfo::exists(directClient))
                return directClient;

            QString nestedClient = QDir::cleanPath(dir.filePath("client/" + clientExe));
            if(QFileInfo::exists(nestedClient))
                return nestedClient;

            QDir toolsDir(dir.filePath("proxmark-tools"));
            if(toolsDir.exists())
            {
                QStringList buildDirs = toolsDir.entryList(QStringList() << "rrg_other-*", QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
                for(const QString& buildDir : qAsConst(buildDirs))
                {
                    QString candidate = QDir::cleanPath(toolsDir.filePath(buildDir + "/client/" + clientExe));
                    if(QFileInfo::exists(candidate))
                        return candidate;
                }
            }

            if(!dir.cdUp())
                break;
        }
    }

    return "";
}

void MainWindow::quickActionsInit()
{
    QToolBar* quickBar = addToolBar(tr("Quick actions"));
    quickBar->setObjectName("quickActionsToolBar");
    quickBar->setMovable(false);
    quickBar->setFloatable(false);

    auto addQuickCommand = [ = ](const QString& label, const QString& command)
    {
        QAction* action = quickBar->addAction(label);
        connect(action, &QAction::triggered, this, [ = ]()
        {
            if(!pm3state)
            {
                QMessageBox::information(this, tr("Info"), tr("Connect to the Proxmark3 first."), QMessageBox::Ok);
                return;
            }
            util->execCMD(command);
            Util::gotoRawTab();
        });
    };

    addQuickCommand(tr("Auto"), "auto");
    addQuickCommand(tr("HF Search"), "hf search");
    addQuickCommand(tr("LF Search"), "lf search");
    addQuickCommand(tr("HW Tune"), "hw tune");
    addQuickCommand(tr("HW Version"), "hw version");
}

void MainWindow::simplePageInit()
{
    if(simpleTab != nullptr)
        return;

    simpleTab = new QWidget(this);
    simpleTab->setObjectName("simpleTab");
    QVBoxLayout* pageLayout = new QVBoxLayout(simpleTab);
    pageLayout->setContentsMargins(18, 18, 18, 18);
    pageLayout->setSpacing(12);

    QGroupBox* connectionBox = new QGroupBox(tr("Connection"), simpleTab);
    QVBoxLayout* connectionOuterLayout = new QVBoxLayout(connectionBox);
    QHBoxLayout* connectionLayout = new QHBoxLayout();
    simplePortBox = new QComboBox(connectionBox);
    simplePortBox->setEditable(true);
    simplePortBox->setMinimumContentsLength(14);
    QPushButton* refreshButton = new QPushButton(tr("Refresh"), connectionBox);
    simpleConnectButton = new QPushButton(tr("Connect"), connectionBox);
    simpleDisconnectButton = new QPushButton(tr("Disconnect"), connectionBox);
    simpleDisconnectButton->setEnabled(false);
    simpleConnectionLabel = new QLabel(tr("Not connected"), connectionBox);
    simpleConnectionLabel->setObjectName("simpleConnectionLabel");
    simpleFirmwareLabel = new QLabel(tr("Firmware check will run after connect."), connectionBox);
    simpleFirmwareLabel->setObjectName("simpleFirmwareLabel");
    simpleFirmwareLabel->setWordWrap(true);
    simpleFirmwareLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    connectionLayout->addWidget(new QLabel(tr("Port:"), connectionBox));
    connectionLayout->addWidget(simplePortBox, 1);
    connectionLayout->addWidget(refreshButton);
    connectionLayout->addWidget(simpleConnectButton);
    connectionLayout->addWidget(simpleDisconnectButton);
    connectionLayout->addWidget(simpleConnectionLabel);
    connectionOuterLayout->addLayout(connectionLayout);
    connectionOuterLayout->addWidget(simpleFirmwareLabel);

    QFrame* scanPanel = new QFrame(simpleTab);
    scanPanel->setObjectName("simpleScanPanel");
    QGridLayout* scanLayout = new QGridLayout(scanPanel);
    scanLayout->setContentsMargins(0, 0, 0, 0);
    scanLayout->setHorizontalSpacing(14);
    scanLayout->setVerticalSpacing(10);

    QFrame* mapFrame = new QFrame(scanPanel);
    mapFrame->setObjectName("simpleReaderMap");
    QVBoxLayout* mapLayout = new QVBoxLayout(mapFrame);
    mapLayout->setContentsMargins(14, 14, 14, 14);
    mapLayout->setSpacing(12);
    QLabel* mapTitle = new QLabel(tr("Proxmark3 placement"), mapFrame);
    mapTitle->setAlignment(Qt::AlignCenter);
    simpleBoardWidget = new SimpleBoardWidget(mapFrame);
    mapLayout->addWidget(mapTitle);
    mapLayout->addWidget(simpleBoardWidget, 1);

    QGroupBox* resultBox = new QGroupBox(tr("Card Scan"), scanPanel);
    QGridLayout* resultLayout = new QGridLayout(resultBox);
    QHBoxLayout* scanActionsLayout = new QHBoxLayout();
    simpleScanModeBox = new QComboBox(resultBox);
    simpleScanModeBox->addItem(tr("Auto"), "auto");
    simpleScanModeBox->addItem(tr("HF/NFC"), "hf");
    simpleScanModeBox->addItem(tr("LF"), "lf");
    simpleScanButton = new QPushButton(tr("Scan Card"), resultBox);
    simpleCloneButton = new QPushButton(tr("Clone"), resultBox);
    simpleDetailsButton = new QPushButton(tr("Show Details"), resultBox);
    simpleCloneButton->setEnabled(false);
    simpleFrequencyLabel = new QLabel("-", resultBox);
    simpleTypeLabel = new QLabel("-", resultBox);
    simpleWritableLabel = new QLabel("-", resultBox);
    simpleIdLabel = new QLabel("-", resultBox);
    simpleIdLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    simpleIdLabel->setWordWrap(true);
    simpleResultLabel = new QLabel(tr("Ready to scan."), resultBox);
    simpleResultLabel->setWordWrap(true);
    scanActionsLayout->addWidget(new QLabel(tr("Mode:"), resultBox));
    scanActionsLayout->addWidget(simpleScanModeBox);
    scanActionsLayout->addWidget(simpleScanButton);
    scanActionsLayout->addWidget(simpleCloneButton, 1);
    resultLayout->addLayout(scanActionsLayout, 0, 0, 1, 3);
    resultLayout->addWidget(new QLabel(tr("Frequency:"), resultBox), 1, 0);
    resultLayout->addWidget(simpleFrequencyLabel, 1, 1, 1, 2);
    resultLayout->addWidget(new QLabel(tr("Type:"), resultBox), 2, 0);
    resultLayout->addWidget(simpleTypeLabel, 2, 1, 1, 2);
    resultLayout->addWidget(new QLabel(tr("Writable:"), resultBox), 3, 0);
    resultLayout->addWidget(simpleWritableLabel, 3, 1, 1, 2);
    resultLayout->addWidget(new QLabel(tr("Card ID:"), resultBox), 4, 0);
    resultLayout->addWidget(simpleIdLabel, 4, 1, 1, 2);
    resultLayout->addWidget(simpleResultLabel, 5, 0, 1, 2);
    resultLayout->addWidget(simpleDetailsButton, 5, 2);
    resultLayout->setColumnStretch(1, 1);

    scanLayout->addWidget(mapFrame, 0, 0);
    scanLayout->addWidget(resultBox, 0, 1);
    scanLayout->setColumnStretch(0, 1);
    scanLayout->setColumnStretch(1, 2);

    simpleDetailsEdit = new QPlainTextEdit(simpleTab);
    simpleDetailsEdit->setObjectName("simpleDetailsEdit");
    simpleDetailsEdit->setReadOnly(true);
    simpleDetailsEdit->setVisible(false);
    simpleDetailsEdit->setMinimumHeight(180);
    simpleDetailsEdit->setPlaceholderText(tr("Raw scan details will appear here."));

    pageLayout->addWidget(connectionBox);
    pageLayout->addWidget(scanPanel);
    pageLayout->addWidget(simpleDetailsEdit, 1);

    QWidget* advancedTab = new QWidget(this);
    advancedTab->setObjectName("advancedTab");
    QVBoxLayout* advancedLayout = new QVBoxLayout(advancedTab);
    advancedLayout->setContentsMargins(0, 0, 0, 0);
    QTabWidget* advancedTabs = new QTabWidget(advancedTab);
    advancedTabs->setObjectName("advancedTabs");
    advancedTabs->setUsesScrollButtons(true);
    advancedLayout->addWidget(advancedTabs);

    ui->funcTab->insertTab(0, simpleTab, tr("Simple"));
    auto moveAdvancedTab = [ = ](QWidget* page, const QString& title)
    {
        int index = ui->funcTab->indexOf(page);
        if(index >= 0)
            ui->funcTab->removeTab(index);
        advancedTabs->addTab(page, title);
    };
    dumpPageInit(advancedTabs);
    moveAdvancedTab(ui->mifareTab, tr("RF / NFC"));
    moveAdvancedTab(ui->lfTab, tr("LF"));
    moveAdvancedTab(t55xxTab, tr("T55xx"));
    moveAdvancedTab(ui->rawTab, tr("Raw"));
    ui->funcTab->insertTab(1, advancedTab, tr("Advanced"));
    ui->funcTab->setTabText(ui->funcTab->indexOf(ui->settingsTab), tr("Settings"));
    ui->funcTab->setUsesScrollButtons(true);
    modernSettingsPageInit();

    ui->label->hide();
    ui->PM3_pathBox->hide();
    ui->label_18->hide();
    ui->PM3_portBox->hide();
    ui->PM3_refreshPortButton->hide();
    ui->PM3_connectButton->hide();
    ui->PM3_disconnectButton->hide();
    ui->label_16->hide();
    ui->Set_UI_setLanguageButton->hide();
    ui->opacityWidget->hide();
    for(QLabel* label : ui->settingsTab->findChildren<QLabel*>())
    {
        if(label->text().contains("new language"))
            label->hide();
    }
    ui->label_67->setText(tr("<a href=\"https://github.com/unicastbg/Proxmark3GUI-Modern/releases\">https://github.com/unicastbg/Proxmark3GUI-Modern/releases</a>"));
    ui->label_67->setOpenExternalLinks(true);
    ui->label_68->hide();
    ui->label_69->hide();

    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::on_PM3_refreshPortButton_clicked);
    connect(simpleConnectButton, &QPushButton::clicked, this, [ = ]()
    {
        QString port = simplePortBox->currentData().toString();
        if(port.isEmpty())
            port = simplePortBox->currentText().remove(" *").trimmed();
        int index = ui->PM3_portBox->findData(port);
        if(index >= 0)
            ui->PM3_portBox->setCurrentIndex(index);
        else
            ui->PM3_portBox->setCurrentText(port);
        on_PM3_connectButton_clicked();
    });
    connect(simpleDisconnectButton, &QPushButton::clicked, this, &MainWindow::on_PM3_disconnectButton_clicked);
    connect(simpleScanButton, &QPushButton::clicked, this, &MainWindow::simpleRunScan);
    connect(simpleCloneButton, &QPushButton::clicked, this, &MainWindow::simpleRunClone);
    connect(simpleDetailsButton, &QPushButton::clicked, this, [ = ]()
    {
        bool showDetails = !simpleDetailsEdit->isVisible();
        simpleDetailsEdit->setVisible(showDetails);
        simpleDetailsButton->setText(showDetails ? tr("Hide Details") : tr("Show Details"));
    });

    simpleSyncPorts();
    simpleHighlightPlacement("");
}

void MainWindow::modernSettingsPageInit()
{
    if(ui->settingsTab->property("modernSettingsBuilt").toBool())
        return;
    ui->settingsTab->setProperty("modernSettingsBuilt", true);

    ui->Set_scrollArea->hide();

    QVBoxLayout* settingsLayout = qobject_cast<QVBoxLayout*>(ui->settingsTab->layout());
    if(settingsLayout == nullptr)
        return;

    QScrollArea* scrollArea = new QScrollArea(ui->settingsTab);
    scrollArea->setObjectName("modernSettingsScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget* content = new QWidget(scrollArea);
    content->setObjectName("modernSettingsContent");
    QVBoxLayout* pageLayout = new QVBoxLayout(content);
    pageLayout->setContentsMargins(18, 18, 18, 18);
    pageLayout->setSpacing(14);

    QLabel* header = new QLabel(tr("Settings"), content);
    QFont headerFont = header->font();
    headerFont.setBold(true);
    headerFont.setPointSize(headerFont.pointSize() + 4);
    header->setFont(headerFont);
    pageLayout->addWidget(header);

    QGroupBox* clientBox = new QGroupBox(tr("Client"), content);
    QFormLayout* clientLayout = new QFormLayout(clientBox);
    clientLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    clientLayout->setLabelAlignment(Qt::AlignRight);
    QLabel* profileLabel = new QLabel(tr("RRG/Iceman modern"), clientBox);
    profileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    clientLayout->addRow(tr("Profile:"), profileLabel);
    clientLayout->addRow(tr("Command config:"), ui->Set_Client_configFileBox);
    QLabel* externalConfigLabel = new QLabel(tr("External config:"), clientBox);
    clientLayout->addRow(externalConfigLabel, ui->Set_Client_configPathEdit);
    pageLayout->addWidget(clientBox);

    auto syncExternalConfigVisibility = [ = ]()
    {
        bool visible = ui->Set_Client_configFileBox->currentData().toString() == "(ext)";
        externalConfigLabel->setVisible(visible);
        ui->Set_Client_configPathEdit->setVisible(visible);
    };
    connect(ui->Set_Client_configFileBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [ = ](int)
    {
        syncExternalConfigVisibility();
    });
    syncExternalConfigVisibility();

    QGroupBox* troubleshootingBox = new QGroupBox(tr("Troubleshooting"), content);
    QVBoxLayout* troubleshootingLayout = new QVBoxLayout(troubleshootingBox);
    QHBoxLayout* keepClientLayout = new QHBoxLayout();
    keepClientLayout->addWidget(ui->Set_Client_keepClientActiveBox);
    QLabel* keepClientLabel = new QLabel(tr("Keep client active if Windows disconnect detection gets in the way."), troubleshootingBox);
    keepClientLabel->setWordWrap(true);
    keepClientLayout->addWidget(keepClientLabel, 1);
    troubleshootingLayout->addLayout(keepClientLayout);

    QHBoxLayout* commandFontLayout = new QHBoxLayout();
    commandFontLayout->addWidget(ui->Set_UI_CMDFont_nameBox, 1);
    commandFontLayout->addWidget(ui->Set_UI_CMDFont_sizeBox);
    commandFontLayout->addWidget(ui->Set_UI_CMDFont_setButton);
    troubleshootingLayout->addWidget(new QLabel(tr("Raw output font:"), troubleshootingBox));
    troubleshootingLayout->addLayout(commandFontLayout);
    pageLayout->addWidget(troubleshootingBox);

    QGroupBox* storageBox = new QGroupBox(tr("Generated Files"), content);
    QVBoxLayout* storageLayout = new QVBoxLayout(storageBox);
    QLabel* storageLabel = new QLabel(storageBox);
    storageLabel->setWordWrap(true);
    QPushButton* cleanAllButton = new QPushButton(tr("Clean Proxmark Dump Files"), storageBox);
    auto updateStorageLabel = [ = ]()
    {
        QFileInfoList files = dumpGeneratedFiles();
        storageLabel->setText(tr("%1 generated hf-mf file(s) in:\n%2")
                              .arg(files.size())
                              .arg(QDir::toNativeSeparators(dumpGeneratedFilesDir().absolutePath())));
    };
    connect(cleanAllButton, &QPushButton::clicked, this, [ = ]()
    {
        QFileInfoList files = dumpGeneratedFiles();
        if(files.isEmpty())
        {
            QMessageBox::information(this, tr("Clean Generated Files"), tr("No generated hf-mf files were found."));
            updateStorageLabel();
            return;
        }

        if(QMessageBox::warning(this, tr("Clean Generated Files"),
                                tr("This will delete %1 generated Proxmark MIFARE key/dump file(s) from:\n%2\n\nThis cannot be undone.")
                                .arg(files.size())
                                .arg(QDir::toNativeSeparators(dumpGeneratedFilesDir().absolutePath())),
                                QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
            return;

        int removed = 0;
        QStringList failed;
        for(const QFileInfo& file : files)
        {
            if(QFile::remove(file.absoluteFilePath()))
                removed++;
            else
                failed << file.fileName();
        }

        updateStorageLabel();
        if(failed.isEmpty())
            QMessageBox::information(this, tr("Clean Generated Files"), tr("Removed %1 generated file(s).").arg(removed));
        else
            QMessageBox::warning(this, tr("Clean Generated Files"), tr("Removed %1 file(s), but could not delete: %2").arg(removed).arg(failed.join(", ")));
    });
    storageLayout->addWidget(storageLabel);
    storageLayout->addWidget(cleanAllButton);
    pageLayout->addWidget(storageBox);
    updateStorageLabel();

    QGroupBox* appearanceBox = new QGroupBox(tr("Appearance"), content);
    QVBoxLayout* appearanceLayout = new QVBoxLayout(appearanceBox);
    QHBoxLayout* themeLayout = new QHBoxLayout();
    themeLayout->addWidget(new QLabel(tr("Theme:"), appearanceBox));
    themeLayout->addWidget(ui->Set_UI_Theme_nameBox, 1);
    ui->Set_UI_Theme_setButton->setText(tr("Save Theme"));
    themeLayout->addWidget(ui->Set_UI_Theme_setButton);
    appearanceLayout->addLayout(themeLayout);
    QLabel* themeHint = new QLabel(tr("Restart the app after saving a theme."), appearanceBox);
    themeHint->setWordWrap(true);
    appearanceLayout->addWidget(themeHint);
    pageLayout->addWidget(appearanceBox);

    QGroupBox* advancedBox = new QGroupBox(tr("Advanced Client Settings"), content);
    advancedBox->setCheckable(true);
    advancedBox->setChecked(false);
    QVBoxLayout* advancedOuterLayout = new QVBoxLayout(advancedBox);
    QWidget* advancedBody = new QWidget(advancedBox);
    QFormLayout* advancedLayout = new QFormLayout(advancedBody);
    advancedLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    advancedLayout->setLabelAlignment(Qt::AlignRight);
    advancedLayout->addRow(tr("Start arguments:"), ui->Set_Client_startArgsEdit);
    advancedLayout->addRow(tr("Working directory:"), ui->Set_Client_workingDirEdit);
    advancedLayout->addRow(tr("Preload script:"), ui->Set_Client_envScriptEdit);
    QHBoxLayout* forceEnabledLayout = new QHBoxLayout();
    forceEnabledLayout->addWidget(ui->Set_Client_forceEnabledBox);
    QLabel* forceEnabledLabel = new QLabel(tr("Keep action buttons enabled while disconnected or busy."), advancedBody);
    forceEnabledLabel->setWordWrap(true);
    forceEnabledLayout->addWidget(forceEnabledLabel, 1);
    advancedLayout->addRow(tr("Developer mode:"), forceEnabledLayout);
    advancedOuterLayout->addWidget(advancedBody);
    advancedBody->setVisible(false);
    connect(advancedBox, &QGroupBox::toggled, advancedBody, &QWidget::setVisible);
    pageLayout->addWidget(advancedBox);

    QGroupBox* aboutBox = new QGroupBox(tr("About"), content);
    QVBoxLayout* aboutLayout = new QVBoxLayout(aboutBox);
    QLabel* appLabel = new QLabel(tr("Modern Windows build for Proxmark3 RRG/Iceman."), aboutBox);
    appLabel->setWordWrap(true);
    QLabel* originalLabel = new QLabel(tr("Original project: <a href=\"https://github.com/wh201906/Proxmark3GUI\">wh201906/Proxmark3GUI</a>"), aboutBox);
    originalLabel->setOpenExternalLinks(true);
    QLabel* licenseLabel = new QLabel(tr("License: LGPL-2.1, matching the bundled upstream license."), aboutBox);
    licenseLabel->setWordWrap(true);
    QLabel* githubLabel = new QLabel(tr("Project: <a href=\"https://github.com/unicastbg/Proxmark3GUI-Modern\">github.com/unicastbg/Proxmark3GUI-Modern</a>"), aboutBox);
    githubLabel->setOpenExternalLinks(true);
    githubLabel->setWordWrap(true);
    aboutLayout->addWidget(appLabel);
    aboutLayout->addWidget(originalLabel);
    aboutLayout->addWidget(licenseLabel);
    aboutLayout->addWidget(githubLabel);
    pageLayout->addWidget(aboutBox);

    pageLayout->addStretch(1);
    scrollArea->setWidget(content);
    settingsLayout->addWidget(scrollArea);
}

void MainWindow::simpleSyncPorts()
{
    if(simplePortBox == nullptr)
        return;

    QString selectedPort = simplePortBox->currentData().toString();
    if(selectedPort.isEmpty())
        selectedPort = simplePortBox->currentText().remove(" *").trimmed();
    QString preferredPort = ui->PM3_portBox->currentData().toString();
    bool preferredIsHinted = ui->PM3_portBox->currentText().endsWith(" *");

    simplePortBox->blockSignals(true);
    simplePortBox->clear();
    for(int i = 0; i < ui->PM3_portBox->count(); i++)
        simplePortBox->addItem(ui->PM3_portBox->itemText(i), ui->PM3_portBox->itemData(i));

    int index = preferredIsHinted ? simplePortBox->findData(preferredPort) : -1;
    if(index < 0)
        index = simplePortBox->findData(selectedPort);
    if(index < 0)
        index = ui->PM3_portBox->currentIndex();
    if(index >= 0)
        simplePortBox->setCurrentIndex(index);
    simplePortBox->blockSignals(false);
}

void MainWindow::simpleSetStatus(const QString& message, const QString& tone)
{
    if(simpleResultLabel == nullptr)
        return;

    simpleResultLabel->setText(message);
    if(tone == "green")
        simpleResultLabel->setStyleSheet("color: #52d273; font-weight: 700;");
    else if(tone == "red")
        simpleResultLabel->setStyleSheet("color: #ff6961; font-weight: 700;");
    else if(tone == "amber")
        simpleResultLabel->setStyleSheet("color: #ffcf5a; font-weight: 700;");
    else
        simpleResultLabel->setStyleSheet("");
}

void MainWindow::simpleSetFirmwareStatus(const QString& message, const QString& tone)
{
    if(simpleFirmwareLabel == nullptr)
        return;

    simpleFirmwareLabel->setText(message);
    if(tone == "green")
        simpleFirmwareLabel->setStyleSheet("color: #52d273; font-weight: 700;");
    else if(tone == "red")
        simpleFirmwareLabel->setStyleSheet("color: #ff6961; font-weight: 700;");
    else if(tone == "amber")
        simpleFirmwareLabel->setStyleSheet("color: #ffcf5a; font-weight: 700;");
    else
        simpleFirmwareLabel->setStyleSheet("");
}

void MainWindow::simpleCheckFirmwareHealth()
{
    if(!pm3state)
        return;
    if(simpleScanButton != nullptr && simpleScanButton->property("busy").toBool())
    {
        QTimer::singleShot(1000, this, &MainWindow::simpleCheckFirmwareHealth);
        return;
    }

    QString output = PM3VersionBar->text() + "\n" + simpleRunCommand("hw version", 3500);
    if(output.trimmed().isEmpty())
    {
        simpleSetFirmwareStatus(tr("Firmware check did not return version details. Scanning may still work."), "amber");
        return;
    }

    QString advice = simpleFirmwareAdvice(output);
    if(advice.startsWith("OK:"))
        simpleSetFirmwareStatus(advice.mid(3).trimmed(), "green");
    else if(advice.startsWith("WARN:"))
        simpleSetFirmwareStatus(advice.mid(5).trimmed(), "amber");
    else
        simpleSetFirmwareStatus(advice, "amber");
}

QString MainWindow::simpleFirmwareAdvice(const QString& output) const
{
    QString lower = output.toLower();
    bool looksIceman = lower.contains("iceman") || lower.contains("rrg");
    if(!looksIceman)
        return tr("WARN: This GUI is tuned for the Iceman/RRG client. Some modern commands may not work with this firmware/client.");

    QString clientVersion = simpleExtractVersionToken(output, "client");
    QString osVersion = simpleExtractVersionToken(output, "os");
    QString bootromVersion = simpleExtractVersionToken(output, "bootrom");

    if(!clientVersion.isEmpty() && !osVersion.isEmpty() && clientVersion != osVersion)
        return tr("WARN: Firmware update recommended. Client build %1 and device OS build %2 do not match.").arg(clientVersion, osVersion);

    if(!clientVersion.isEmpty() && !bootromVersion.isEmpty() && clientVersion != bootromVersion)
        return tr("WARN: Client and bootrom builds differ (%1 vs %2). If scans fail, update the device firmware.").arg(clientVersion, bootromVersion);

    if(!osVersion.isEmpty())
        return tr("OK: Iceman/RRG firmware detected (%1).").arg(osVersion);

    QRegularExpression versionPattern("\\bv\\d+(?:\\.\\d+)+(?:[-.][A-Za-z0-9]+)*\\b", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch versionMatch = versionPattern.match(output);
    if(versionMatch.hasMatch())
        return tr("OK: Iceman/RRG firmware detected (%1).").arg(versionMatch.captured(0));

    return tr("WARN: Iceman/RRG client detected, but the device firmware build could not be read clearly. Update if commands behave oddly.");
}

QString MainWindow::simpleExtractVersionToken(const QString& text, const QString& label) const
{
    QRegularExpression linePattern("^\\s*" + QRegularExpression::escape(label) + "\\s*[:.]+\\s*([^\\r\\n]+)",
                                   QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
    QRegularExpressionMatch lineMatch = linePattern.match(text);
    if(!lineMatch.hasMatch())
        return "";

    QString line = lineMatch.captured(1).trimmed();
    QRegularExpression versionPattern("\\bv\\d+(?:\\.\\d+)+(?:[-.][A-Za-z0-9]+)*\\b", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch versionMatch = versionPattern.match(line);
    if(versionMatch.hasMatch())
        return versionMatch.captured(0);

    QRegularExpression datePattern("\\b\\d{4}-\\d{2}-\\d{2}\\b");
    QRegularExpressionMatch dateMatch = datePattern.match(line);
    if(dateMatch.hasMatch())
        return dateMatch.captured(0);

    return line.section(' ', 0, 0);
}

void MainWindow::simpleSetBusy(bool busy)
{
    setState(!busy);
    if(simpleScanButton != nullptr)
    {
        simpleScanButton->setProperty("busy", busy);
        simpleScanButton->setText(busy ? tr("Scanning...") : tr("Scan Card"));
    }
}

void MainWindow::simpleClearScanDisplay()
{
    simpleLastScanOutput.clear();
    simpleLastCardId.clear();
    simpleLastCardFamily.clear();
    simpleSetStatus(tr("Scanning for a card..."), "");
    if(simpleFrequencyLabel != nullptr)
        simpleFrequencyLabel->setText("-");
    if(simpleTypeLabel != nullptr)
        simpleTypeLabel->setText("-");
    if(simpleWritableLabel != nullptr)
        simpleWritableLabel->setText("-");
    if(simpleIdLabel != nullptr)
        simpleIdLabel->setText("-");
    if(simpleDetailsEdit != nullptr)
        simpleDetailsEdit->clear();
    if(simpleCloneButton != nullptr)
        simpleCloneButton->setEnabled(false);
    simpleHighlightPlacement("");
}

QString MainWindow::simpleRunCommand(const QString& command, int waitTime)
{
    return util->execCMDWithOutput(command, Util::ReturnTrigger(static_cast<unsigned long>(waitTime)), true);
}

void MainWindow::simpleRunScan()
{
    if(!pm3state)
    {
        simpleSetStatus(tr("Connect to the Proxmark3 first."), "amber");
        return;
    }

    simpleClearScanDisplay();
    simpleSetBusy(true);
    QString mode = (simpleScanModeBox == nullptr) ? "auto" : simpleScanModeBox->currentData().toString();
    if(mode.isEmpty())
        mode = "auto";
    QString output;
    QString family;
    QString id;

    if(mode != "lf")
    {
        simpleSetStatus(tr("Checking HF/NFC first..."), "");
        output += "--- hf 14a info ---\n" + simpleRunCommand("hf 14a info", 2500);
        family = simpleDetectFamily(output);
        id = simpleExtractCardId(output, family);
        if(family != "Unknown" && !id.isEmpty())
        {
            simpleRunCommand("data hide", 500);
            simpleApplyScanOutput(output);
            simpleSetBusy(false);
            return;
        }

        simpleSetStatus(tr("Checking broader HF/NFC scan..."), "");
        output += "\n\n--- hf search ---\n" + simpleRunCommand("hf search", 3500);
        family = simpleDetectFamily(output);
        id = simpleExtractCardId(output, family);
        if(simpleFrequencyForFamily(family).startsWith("HF") && !id.isEmpty())
        {
            simpleRunCommand("data hide", 500);
            simpleApplyScanOutput(output);
            simpleSetBusy(false);
            return;
        }

        if(mode == "hf")
        {
            simpleRunCommand("data hide", 500);
            simpleApplyScanOutput(output);
            simpleSetBusy(false);
            return;
        }
    }

    if(mode != "hf")
    {
        simpleSetStatus(tr("Checking LF pad..."), "");
        if(!output.isEmpty())
            output += "\n\n";
        output += "--- lf search ---\n" + simpleRunCommand("lf search", 4500);
        family = simpleDetectFamily(output);
        id = simpleExtractCardId(output, family);
        if(family != "Unknown" && !id.isEmpty())
        {
            simpleRunCommand("data hide", 500);
            simpleApplyScanOutput(output);
            simpleSetBusy(false);
            return;
        }

        if(mode == "lf")
        {
            simpleRunCommand("data hide", 500);
            simpleApplyScanOutput(output);
            simpleSetBusy(false);
            return;
        }
    }

    simpleSetStatus(tr("Trying broad auto-detection..."), "");
    output += "\n\n--- auto ---\n" + simpleRunCommand("auto", 5000);
    simpleRunCommand("data hide", 500);
    simpleApplyScanOutput(output);
    simpleSetBusy(false);
}

void MainWindow::simpleRunClone()
{
    if(!pm3state)
    {
        simpleSetStatus(tr("Connect to the Proxmark3 first."), "amber");
        return;
    }

    QStringList cloneCommands = simpleCloneCommands();
    if(cloneCommands.isEmpty())
    {
        simpleSetStatus(tr("Automated cloning for this card type is not mapped yet. Use the Advanced tabs for this one."), "amber");
        return;
    }

    QString readCommand = simpleReadCommandForFamily(simpleLastCardFamily);
    QString targetPrompt = simpleCloneTargetPrompt();
    if(readCommand.isEmpty() || targetPrompt.isEmpty())
    {
        simpleSetStatus(tr("Automated cloning for this card type is not mapped yet. Use the Advanced tabs for this one."), "amber");
        return;
    }

    simpleSetStatus(tr("Clone started. Keep the original card on the reader for the source check."), "");

    int confirm = QMessageBox::question(this, tr("Clone Card"),
                                        tr("Only continue with a card you own or are authorized to copy.\n\nThis Simple Clone flow copies the visible card ID/UID. It does not copy sectors or pages that require authentication.\n\nLeave the original card on the reader for one more check."),
                                        QMessageBox::Ok | QMessageBox::Cancel);
    if(confirm != QMessageBox::Ok)
    {
        simpleSetStatus(tr("Clone cancelled. The last scanned card is still ready."), "amber");
        return;
    }

    simpleSetBusy(true);
    simpleSetStatus(tr("Checking the source card..."), "");
    QString originalCheck = simpleRunCommand(readCommand, 5000);
    QString checkedId = simpleExtractCardId(originalCheck, simpleLastCardFamily);
    if(checkedId != simpleLastCardId)
    {
        simpleDetailsEdit->setPlainText(originalCheck);
        simpleSetStatus(tr("Source card check did not match the first scan. Clone stopped."), "red");
        simpleSetBusy(false);
        return;
    }

    simpleSetBusy(false);
    simpleSetStatus(tr("Source verified. Waiting for the target card..."), "");
    QMessageBox::information(this, tr("Clone Card"), targetPrompt);
    simpleSetBusy(true);
    simpleSetStatus(tr("Writing target card and verifying..."), "");

    QString cloneOutput;
    QString verifyOutput;
    QString verifyId;
    for(const QString& command : cloneCommands)
    {
        simpleSetStatus(tr("Trying clone method %1 of %2...").arg(cloneCommands.indexOf(command) + 1).arg(cloneCommands.size()), "");
        cloneOutput += "\n\n--- clone attempt: " + command + " ---\n" + simpleRunCommand(command, 10000);
        verifyOutput = simpleRunCommand(readCommand, 5000);
        verifyId = simpleExtractCardId(verifyOutput, simpleLastCardFamily);
        cloneOutput += "\n\n--- verify after attempt ---\n" + verifyOutput;
        if(verifyId == simpleLastCardId)
            break;
    }
    simpleDetailsEdit->setPlainText((originalCheck + cloneOutput).trimmed());

    if(verifyId == simpleLastCardId)
        simpleSetStatus(tr("Clone verified. The readable ID matches the original."), "green");
    else
        simpleSetStatus(tr("Clone failed verification. The readable ID does not match. The target may not support UID/block-0 writes; use a compatible magic card."), "red");

    simpleSetBusy(false);
}

void MainWindow::simpleApplyScanOutput(const QString& output)
{
    simpleLastScanOutput = output;
    simpleLastCardFamily = simpleDetectFamily(output);
    simpleLastCardId = simpleExtractCardId(output, simpleLastCardFamily);
    QString frequency = simpleFrequencyForFamily(simpleLastCardFamily);

    simpleFrequencyLabel->setText(frequency.isEmpty() ? tr("Unknown") : frequency);
    simpleTypeLabel->setText(simpleLastCardFamily);
    simpleWritableLabel->setText(simpleWritableForFamily(simpleLastCardFamily));
    simpleIdLabel->setText(simpleLastCardId.isEmpty() ? tr("Not found") : simpleLastCardId);
    simpleDetailsEdit->setPlainText(output.trimmed());
    simpleCloneButton->setEnabled(pm3state && !simpleCloneCommand().isEmpty());
    simpleHighlightPlacement(frequency);

    if(simpleLastCardFamily == "Unknown")
        simpleSetStatus(tr("No recognizable card was detected. Try moving it on the LF or HF/NFC pad and scan again."), "amber");
    else if(simpleLastCardId.isEmpty())
        simpleSetStatus(tr("Card type detected, but no stable visible ID was parsed yet. Press Show Details to inspect the raw scan."), "amber");
    else if(simpleIsHfUidCloneFamily(simpleLastCardFamily))
        simpleSetStatus(tr("NFC chip identified. Simple Clone copies the visible UID. For full card backup/restore, use Advanced > Guided Dump."), "amber");
    else
        simpleSetStatus(tr("Card detected. The visible ID is ready for comparison."), "green");
}

QString MainWindow::simpleExtractCardId(const QString& output, const QString& family) const
{
    QString text = output;
    if(family == "HID Prox")
    {
        QRegularExpression hidPattern("FC\\s*[:= ]\\s*(\\d+).*CN\\s*[:= ]\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch hidMatch = hidPattern.match(text);
        if(hidMatch.hasMatch())
            return "FC " + hidMatch.captured(1) + " / CN " + hidMatch.captured(2);
    }

    QStringList patterns;
    if(family == "EM410x")
    {
        patterns << "EM\\s*(?:410x|4100|TAG).*?(?:ID)?\\s*[:= ]+([0-9A-Fa-f]{10})"
                 << "(?:ID|Tag ID)\\s*[:= ]+([0-9A-Fa-f]{10})";
    }
    patterns << "(?:UID|CSN|Card ID|ID)\\s*(?:\\([^)]+\\))?\\s*[:= ]+([0-9A-Fa-f][0-9A-Fa-f\\s:.-]{3,40})"
             << "\\b([0-9A-Fa-f]{10})\\b"
             << "\\b([0-9A-Fa-f]{8})\\b";

    for(const QString& pattern : patterns)
    {
        QRegularExpression re(pattern, QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = re.match(text);
        if(match.hasMatch())
        {
            QString id = match.captured(1).toUpper();
            id.remove(QRegularExpression("[^0-9A-F]"));
            if(id.length() >= 4)
                return id;
        }
    }
    return "";
}

QString MainWindow::simpleDetectFamily(const QString& output) const
{
    QString lower = output.toLower();
    if(lower.contains("em 410x") || lower.contains("em410") || lower.contains("em tag"))
        return "EM410x";
    if(lower.contains("hid prox") || lower.contains("hid h103") || lower.contains("hid format"))
        return "HID Prox";
    if(lower.contains("mifare classic"))
        return "MIFARE Classic";
    if(lower.contains("ntag"))
        return "NTAG";
    if(lower.contains("iso14443-a") || lower.contains("iso 14443-a") || (lower.contains("uid") && lower.contains("atqa") && lower.contains("sak")))
        return "ISO14443-A / NFC";
    if(lower.contains("t55") || lower.contains("t5577"))
        return "T55xx";
    if(lower.contains("lf search") || lower.contains("low frequency"))
        return "LF tag";
    return "Unknown";
}

QString MainWindow::simpleFrequencyForFamily(const QString& family) const
{
    if(family == "EM410x" || family == "HID Prox" || family == "T55xx" || family == "LF tag")
        return tr("LF 125 / 134 kHz");
    if(family == "MIFARE Classic" || family == "NTAG" || family == "ISO14443-A / NFC")
        return tr("HF / NFC 13.56 MHz");
    return "";
}

QString MainWindow::simpleWritableForFamily(const QString& family) const
{
    if(family == "EM410x" || family == "HID Prox")
        return tr("Usually read-only; clone needs a writable T5577-compatible target.");
    if(family == "MIFARE Classic")
        return tr("Simple Clone writes UID only; full sector copy requires readable keys and a compatible target.");
    if(family == "NTAG")
        return tr("Readable pages depend on lock/password settings and target tag compatibility.");
    if(family == "ISO14443-A / NFC")
        return tr("Chip identified; Simple Clone writes UID only to a compatible HF magic card.");
    if(family == "T55xx")
        return tr("Writable LF tag family.");
    return tr("Unknown");
}

QString MainWindow::simpleCloneCommand() const
{
    QStringList commands = simpleCloneCommands();
    return commands.isEmpty() ? QString() : commands.first();
}

QStringList MainWindow::simpleCloneCommands() const
{
    QStringList commands;
    if(simpleLastCardFamily == "EM410x" && simpleLastCardId.length() == 10)
    {
        commands << "lf em 410x clone --id " + simpleLastCardId;
        return commands;
    }

    if(simpleIsHfUidCloneFamily(simpleLastCardFamily) && (simpleLastCardId.length() == 8 || simpleLastCardId.length() == 14 || simpleLastCardId.length() == 20))
    {
        QString command = "hf mf csetuid --uid " + simpleLastCardId;
        QString atqa = simpleExtractHfParameter(simpleLastScanOutput, "ATQA");
        QString sak = simpleExtractHfParameter(simpleLastScanOutput, "SAK");
        if(!atqa.isEmpty())
            command += " --atqa " + atqa;
        if(!sak.isEmpty())
            command += " --sak " + sak;
        commands << command;

        QString block0 = simpleBuildMifareBlock0(simpleLastCardId, atqa, sak);
        if(!block0.isEmpty())
        {
            commands << "hf mf wrbl --blk 0 -a -k FFFFFFFFFFFF -d " + block0 + " --force";
            commands << "hf mf wrbl --blk 0 -b -k FFFFFFFFFFFF -d " + block0 + " --force";
        }

        commands << "script run hf_mf_uscuid_prog -t 4 -u " + simpleLastCardId;
        commands << "script run hf_mf_uscuid_prog -t 2 -u " + simpleLastCardId;
        return commands;
    }
    return commands;
}

QString MainWindow::simpleReadCommandForFamily(const QString& family) const
{
    if(family == "EM410x")
        return "lf em 410x reader";
    if(simpleIsHfUidCloneFamily(family) || family == "NTAG")
        return "hf 14a info";
    return "";
}

QString MainWindow::simpleCloneTargetPrompt() const
{
    if(simpleLastCardFamily == "EM410x")
        return tr("Now place a writable T5577-compatible LF tag on the LF pad, then press OK.");
    if(simpleIsHfUidCloneFamily(simpleLastCardFamily))
        return tr("Now place a compatible HF magic UID card on the RF/HF/NFC pad, then press OK.\n\nThis writes the visible UID and then reads it back for comparison.");
    return "";
}

bool MainWindow::simpleIsHfUidCloneFamily(const QString& family) const
{
    return family == "ISO14443-A / NFC" || family == "MIFARE Classic";
}

QString MainWindow::simpleExtractHfParameter(const QString& output, const QString& parameter) const
{
    QRegularExpression re(parameter + "\\s*[:=]\\s*((?:0x)?[0-9A-Fa-f]{2}(?:[\\s:.-]+(?:0x)?[0-9A-Fa-f]{2}){0,3}|(?:0x)?[0-9A-Fa-f]{2,8})", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = re.match(output);
    if(!match.hasMatch())
        return "";

    QString value = match.captured(1).toUpper();
    value.remove(QRegularExpression("[^0-9A-F]"));
    return value;
}

QString MainWindow::simpleBuildMifareBlock0(const QString& uid, const QString& atqa, const QString& sak) const
{
    QString cleanUid = uid.toUpper();
    cleanUid.remove(QRegularExpression("[^0-9A-F]"));
    if(cleanUid.length() != 8)
        return "";

    bool ok = false;
    quint8 bcc = 0;
    for(int i = 0; i < cleanUid.length(); i += 2)
    {
        quint8 byte = static_cast<quint8>(cleanUid.mid(i, 2).toUInt(&ok, 16));
        if(!ok)
            return "";
        bcc ^= byte;
    }

    QString cleanSak = sak.toUpper();
    cleanSak.remove(QRegularExpression("[^0-9A-F]"));
    if(cleanSak.length() < 2)
        cleanSak = "08";
    cleanSak = cleanSak.right(2);

    QString cleanAtqa = atqa.toUpper();
    cleanAtqa.remove(QRegularExpression("[^0-9A-F]"));
    if(cleanAtqa.length() < 4)
        cleanAtqa = "0004";
    cleanAtqa = cleanAtqa.right(4);
    QString atqaLittleEndian = cleanAtqa.mid(2, 2) + cleanAtqa.mid(0, 2);

    return cleanUid + QString("%1").arg(bcc, 2, 16, QLatin1Char('0')).toUpper() +
           cleanSak + atqaLittleEndian + "00000000000000000000";
}

void MainWindow::simpleHighlightPlacement(const QString& frequency)
{
    if(simpleBoardWidget == nullptr)
        return;

    if(frequency.startsWith("LF"))
    {
        simpleBoardWidget->setProperty("activeArea", "lf");
        simpleBoardWidget->setProperty("placementText", tr("Use the LF pad for this card."));
    }
    else if(frequency.startsWith("HF"))
    {
        simpleBoardWidget->setProperty("activeArea", "hf");
        simpleBoardWidget->setProperty("placementText", tr("Use the RF/HF/NFC pad for this card."));
    }
    else
    {
        simpleBoardWidget->setProperty("activeArea", "");
        simpleBoardWidget->setProperty("placementText", tr("Place the card on the reader, then scan."));
    }
    simpleBoardWidget->update();
}

void MainWindow::dumpPageInit(QTabWidget* advancedTabs)
{
    if(advancedTabs == nullptr || dumpTab != nullptr)
        return;

    dumpTab = new QWidget(advancedTabs);
    dumpTab->setObjectName("guidedDumpTab");
    QVBoxLayout* outerLayout = new QVBoxLayout(dumpTab);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scrollArea = new QScrollArea(dumpTab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    QWidget* content = new QWidget(scrollArea);
    QVBoxLayout* pageLayout = new QVBoxLayout(content);
    pageLayout->setContentsMargins(18, 18, 18, 18);
    pageLayout->setSpacing(12);

    QGroupBox* flowBox = new QGroupBox(tr("Guided Dump + Restore"), content);
    QGridLayout* flowLayout = new QGridLayout(flowBox);
    flowLayout->setHorizontalSpacing(10);
    flowLayout->setVerticalSpacing(10);

    dumpModeBox = new QComboBox(flowBox);
    dumpModeBox->addItem(tr("Auto"), "auto");
    dumpModeBox->addItem(tr("HF/NFC"), "hf");
    dumpModeBox->addItem(tr("LF/RFID"), "lf");

    dumpMifareSizeBox = new QComboBox(flowBox);
    dumpMifareSizeBox->addItem(tr("MIFARE size: Auto/1K"), "1k");
    dumpMifareSizeBox->addItem(tr("MIFARE Mini"), "mini");
    dumpMifareSizeBox->addItem(tr("MIFARE 1K"), "1k");
    dumpMifareSizeBox->addItem(tr("MIFARE 2K"), "2k");
    dumpMifareSizeBox->addItem(tr("MIFARE 4K"), "4k");

    dumpDetectButton = new QPushButton(tr("1. Detect Card"), flowBox);
    dumpReadButton = new QPushButton(tr("2. Dump / Read"), flowBox);
    dumpWriteButton = new QPushButton(tr("3. Write Target"), flowBox);
    dumpVerifyButton = new QPushButton(tr("4. Verify"), flowBox);
    dumpCleanButton = new QPushButton(tr("Clean Files"), flowBox);
    dumpReadButton->setEnabled(false);
    dumpWriteButton->setEnabled(false);
    dumpVerifyButton->setEnabled(false);
    dumpCleanButton->setEnabled(false);

    flowLayout->addWidget(new QLabel(tr("Mode:"), flowBox), 0, 0);
    flowLayout->addWidget(dumpModeBox, 0, 1);
    flowLayout->addWidget(dumpMifareSizeBox, 0, 2);
    flowLayout->addWidget(dumpDetectButton, 1, 0);
    flowLayout->addWidget(dumpReadButton, 1, 1);
    flowLayout->addWidget(dumpWriteButton, 1, 2);
    flowLayout->addWidget(dumpVerifyButton, 1, 3);
    flowLayout->addWidget(dumpCleanButton, 1, 4);
    flowLayout->setColumnStretch(5, 1);

    QGroupBox* summaryBox = new QGroupBox(tr("Detected Card"), content);
    QGridLayout* summaryLayout = new QGridLayout(summaryBox);
    dumpFamilyLabel = new QLabel("-", summaryBox);
    dumpFrequencyLabel = new QLabel("-", summaryBox);
    dumpIdLabel = new QLabel("-", summaryBox);
    dumpCapabilityLabel = new QLabel(tr("Detect a card first."), summaryBox);
    dumpStatusLabel = new QLabel(tr("Ready."), summaryBox);
    dumpIdLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    dumpCapabilityLabel->setWordWrap(true);
    dumpStatusLabel->setWordWrap(true);
    summaryLayout->addWidget(new QLabel(tr("Type:"), summaryBox), 0, 0);
    summaryLayout->addWidget(dumpFamilyLabel, 0, 1);
    summaryLayout->addWidget(new QLabel(tr("Frequency:"), summaryBox), 1, 0);
    summaryLayout->addWidget(dumpFrequencyLabel, 1, 1);
    summaryLayout->addWidget(new QLabel(tr("Visible ID:"), summaryBox), 2, 0);
    summaryLayout->addWidget(dumpIdLabel, 2, 1);
    summaryLayout->addWidget(new QLabel(tr("Support:"), summaryBox), 3, 0);
    summaryLayout->addWidget(dumpCapabilityLabel, 3, 1);
    summaryLayout->addWidget(dumpStatusLabel, 4, 0, 1, 2);
    summaryLayout->setColumnStretch(1, 1);

    dumpFieldTable = new QTableWidget(0, 2, content);
    dumpFieldTable->setObjectName("guidedDumpFieldTable");
    dumpFieldTable->setHorizontalHeaderLabels(QStringList() << tr("Field") << tr("Value"));
    dumpFieldTable->horizontalHeader()->setStretchLastSection(true);
    dumpFieldTable->verticalHeader()->hide();
    dumpFieldTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    dumpFieldTable->setSelectionMode(QAbstractItemView::SingleSelection);
    dumpFieldTable->setMinimumHeight(150);

    QGroupBox* mapBox = new QGroupBox(tr("Card Map"), content);
    QVBoxLayout* mapLayout = new QVBoxLayout(mapBox);
    dumpMapTable = new QTableWidget(0, 6, mapBox);
    dumpMapTable->setObjectName("guidedDumpMapTable");
    dumpMapTable->setHorizontalHeaderLabels(QStringList() << tr("Sector") << tr("Block") << tr("Role") << tr("Source Data") << tr("Target Data") << tr("Status"));
    dumpMapTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    dumpMapTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    dumpMapTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    dumpMapTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    dumpMapTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    dumpMapTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    dumpMapTable->verticalHeader()->hide();
    dumpMapTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    dumpMapTable->setSelectionMode(QAbstractItemView::SingleSelection);
    dumpMapTable->setMinimumHeight(260);
    dumpMapTable->setContextMenuPolicy(Qt::ActionsContextMenu);
    QAction* copyMapRowAction = new QAction(tr("Copy selected row"), dumpMapTable);
    dumpMapTable->addAction(copyMapRowAction);
    connect(copyMapRowAction, &QAction::triggered, this, [this]()
    {
        if(dumpMapTable == nullptr)
            return;
        int row = dumpMapTable->currentRow();
        if(row < 0)
            return;

        QStringList values;
        for(int column = 0; column < dumpMapTable->columnCount(); column++)
        {
            QTableWidgetItem* item = dumpMapTable->item(row, column);
            values << (item == nullptr ? QString() : item->text());
        }
        QApplication::clipboard()->setText(values.join('\t'));
    });
    mapLayout->addWidget(dumpMapTable);

    dumpOutputEdit = new QPlainTextEdit(content);
    dumpOutputEdit->setReadOnly(true);
    dumpOutputEdit->setMinimumHeight(260);
    dumpOutputEdit->setPlaceholderText(tr("Detection, dump, write, and verify output will appear here."));

    pageLayout->addWidget(flowBox);
    pageLayout->addWidget(summaryBox);
    pageLayout->addWidget(dumpFieldTable);
    pageLayout->addWidget(mapBox);
    pageLayout->addWidget(dumpOutputEdit, 1);
    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
    advancedTabs->addTab(dumpTab, tr("Guided Dump"));

    connect(dumpDetectButton, &QPushButton::clicked, this, &MainWindow::dumpRunDetect);
    connect(dumpReadButton, &QPushButton::clicked, this, &MainWindow::dumpRunRead);
    connect(dumpWriteButton, &QPushButton::clicked, this, &MainWindow::dumpRunWrite);
    connect(dumpVerifyButton, &QPushButton::clicked, this, &MainWindow::dumpRunVerify);
    connect(dumpCleanButton, &QPushButton::clicked, this, &MainWindow::dumpRunCleanFiles);
}

void MainWindow::dumpSetStatus(const QString& message, const QString& tone)
{
    if(dumpStatusLabel == nullptr)
        return;

    dumpStatusLabel->setText(message);
    if(tone == "green")
        dumpStatusLabel->setStyleSheet("color: #52d273; font-weight: 700;");
    else if(tone == "red")
        dumpStatusLabel->setStyleSheet("color: #ff6961; font-weight: 700;");
    else if(tone == "amber")
        dumpStatusLabel->setStyleSheet("color: #ffcf5a; font-weight: 700;");
    else
        dumpStatusLabel->setStyleSheet("");
}

void MainWindow::dumpSetBusy(bool busy)
{
    setState(!busy);
    if(dumpDetectButton != nullptr)
        dumpDetectButton->setEnabled(!busy && pm3state);
    if(dumpReadButton != nullptr)
        dumpReadButton->setEnabled(!busy && pm3state && dumpLastFamily != "Unknown" && !dumpLastFamily.isEmpty());
    if(dumpWriteButton != nullptr)
        dumpWriteButton->setEnabled(!busy && pm3state && !dumpReadOutput.trimmed().isEmpty() && dumpCanWriteFamily(dumpLastFamily));
    if(dumpVerifyButton != nullptr)
        dumpVerifyButton->setEnabled(!busy && pm3state && !dumpWriteOutput.trimmed().isEmpty() && dumpCanWriteFamily(dumpLastFamily));
    if(dumpCleanButton != nullptr)
        dumpCleanButton->setEnabled(!busy && !dumpLastCardId.isEmpty());
}

void MainWindow::dumpClearDisplay()
{
    dumpSourceOutput.clear();
    dumpReadOutput.clear();
    dumpWriteOutput.clear();
    dumpVerifyOutput.clear();
    dumpLastFamily.clear();
    dumpLastCardId.clear();
    dumpSourceBlockMap.clear();
    dumpTargetBlockMap.clear();
    dumpSetStatus(tr("Detecting card..."), "");
    if(dumpFamilyLabel != nullptr)
        dumpFamilyLabel->setText("-");
    if(dumpFrequencyLabel != nullptr)
        dumpFrequencyLabel->setText("-");
    if(dumpIdLabel != nullptr)
        dumpIdLabel->setText("-");
    if(dumpCapabilityLabel != nullptr)
        dumpCapabilityLabel->setText(tr("Detecting card..."));
    if(dumpFieldTable != nullptr)
        dumpFieldTable->setRowCount(0);
    if(dumpMapTable != nullptr)
        dumpMapTable->setRowCount(0);
    if(dumpOutputEdit != nullptr)
        dumpOutputEdit->clear();
    if(dumpReadButton != nullptr)
        dumpReadButton->setEnabled(false);
    if(dumpWriteButton != nullptr)
        dumpWriteButton->setEnabled(false);
    if(dumpVerifyButton != nullptr)
        dumpVerifyButton->setEnabled(false);
    if(dumpCleanButton != nullptr)
        dumpCleanButton->setEnabled(false);
}

void MainWindow::dumpRunDetect()
{
    if(!pm3state)
    {
        dumpSetStatus(tr("Connect to the Proxmark3 first."), "amber");
        return;
    }

    dumpClearDisplay();
    dumpSetBusy(true);

    QString mode = dumpModeBox == nullptr ? "auto" : dumpModeBox->currentData().toString();
    QString output;
    if(mode != "lf")
    {
        dumpSetStatus(tr("Checking HF/NFC..."), "");
        output += "--- hf 14a info ---\n" + simpleRunCommand("hf 14a info", 3000);
        QString infoFamily = simpleDetectFamily(output);
        if(infoFamily == "Unknown" || !simpleFrequencyForFamily(infoFamily).startsWith("HF"))
            output += "\n\n--- hf search ---\n" + simpleRunCommand("hf search", 4500);
    }

    QString hfFamily = simpleDetectFamily(output);
    bool hfCardDetected = !output.isEmpty() && hfFamily != "Unknown" && simpleFrequencyForFamily(hfFamily).startsWith("HF");
    if(mode != "hf" && !hfCardDetected)
    {
        dumpSetStatus(tr("Checking LF/RFID..."), "");
        if(!output.isEmpty())
            output += "\n\n";
        output += "--- lf search ---\n" + simpleRunCommand("lf search", 5000);
    }

    simpleRunCommand("data hide", 500);
    dumpApplyDetectOutput(output);
    dumpSetBusy(false);
}

void MainWindow::dumpRunRead()
{
    if(!pm3state)
    {
        dumpSetStatus(tr("Connect to the Proxmark3 first."), "amber");
        return;
    }

    dumpSetBusy(true);
    QString output;
    if(dumpLastFamily == "MIFARE Classic")
    {
        QString cardArg = dumpMifareCardArg();
        QString keyCheckCommand = "hf mf chk --" + cardArg + " --dump";
        QString dumpCommand = "hf mf dump --" + cardArg + dumpMifareFileArgs();

        bool keyFileAvailable = false;
        const QString keyFileName = dumpMifareKeyFilename();
        for(const QFileInfo& file : dumpGeneratedFiles(dumpLastCardId))
        {
            if(file.fileName().compare(keyFileName, Qt::CaseInsensitive) == 0)
            {
                keyFileAvailable = true;
                break;
            }
        }

        if(keyFileAvailable)
        {
            dumpSetStatus(tr("Known key file found. Trying direct MIFARE dump first."), "");
            dumpMirrorMifareKeyFile();
            QString directDumpOutput = simpleRunCommand(dumpCommand, 12000);
            QString lower = directDumpOutput.toLower();
            bool directFailed = lower.contains("can't find") ||
                                lower.contains("file not found") ||
                                lower.contains("failed") ||
                                lower.contains("auth error") ||
                                lower.contains("no known key");
            output = "--- hf mf dump using existing keys ---\n--- " + dumpCommand + " ---\n" + directDumpOutput;
            if(directFailed)
                output += "\n\n--- direct dump did not finish cleanly; checking keys next ---\n";
        }

        if(output.isEmpty() || output.contains("direct dump did not finish cleanly", Qt::CaseInsensitive))
        {
            dumpSetStatus(tr("Checking MIFARE Classic keys, then dumping readable sectors."), "");
            QString keyCheckOutput = simpleRunCommand(keyCheckCommand, 20000);
            dumpMirrorMifareKeyFile();
            if(!output.isEmpty())
                output += "\n\n";
            output += QString("--- hf mf chk / dump ---\n") +
                      "--- " + keyCheckCommand + " ---\n" + keyCheckOutput +
                      "\n\n--- " + dumpCommand + " ---\n" + simpleRunCommand(dumpCommand, 25000);
        }
    }
    else if(dumpLastFamily == "EM410x")
    {
        dumpSetStatus(tr("Reading LF EM410x ID..."), "");
        output = "--- lf em 410x reader ---\n" + simpleRunCommand("lf em 410x reader", 6000);
    }
    else if(simpleFrequencyForFamily(dumpLastFamily).startsWith("LF"))
    {
        dumpSetStatus(tr("Capturing LF raw data. Automatic writing is not mapped for this family yet."), "amber");
        output = "--- lf read -v ---\n" + simpleRunCommand("lf read -v", 7000) +
                 "\n\n--- data samples ---\n" + simpleRunCommand("data samples", 3000);
    }
    else if(dumpLastFamily == "NTAG")
    {
        dumpSetStatus(tr("Reading NTAG/Ultralight pages. Restore is not automated yet because locks/counters/passwords vary by tag."), "amber");
        output = "--- hf mfu info ---\n" + simpleRunCommand("hf mfu info", 5000) +
                 "\n\n--- hf mfu dump ---\n" + simpleRunCommand("hf mfu dump", 12000);
    }
    else
    {
        dumpSetStatus(tr("This card type can be identified, but a full dump/write recipe is not mapped yet."), "amber");
        output = dumpSourceOutput;
    }

    dumpReadOutput = output;
    dumpWriteOutput.clear();
    dumpVerifyOutput.clear();
    dumpSourceBlockMap = dumpLoadMifareBlockMapFromLatestFile();
    if(dumpSourceBlockMap.isEmpty())
        dumpSourceBlockMap = dumpExtractMifareBlockMap(dumpReadOutput);
    dumpTargetBlockMap.clear();
    dumpOutputEdit->setPlainText(output.trimmed());
    dumpRefreshFields();
    if(!dumpCanWriteFamily(dumpLastFamily))
        dumpWriteOutput.clear();
    dumpSetBusy(false);
}

void MainWindow::dumpRunWrite()
{
    if(!pm3state)
    {
        dumpSetStatus(tr("Connect to the Proxmark3 first."), "amber");
        return;
    }

    QString prompt;
    if(dumpLastFamily == "EM410x")
        prompt = tr("Only continue with a card you own or are authorized to copy.\n\nPlace a writable T5577-compatible LF tag on the LF pad, then press OK.");
    else if(dumpLastFamily == "MIFARE Classic")
        prompt = tr("Only continue with a card you own or are authorized to copy.\n\nPlace a compatible MIFARE Classic magic target card on the HF/NFC pad, then press OK.\n\nThe restore uses the dump file produced by the Proxmark client.");
    else
    {
        dumpSetStatus(tr("Writing is not mapped for this card family yet."), "amber");
        return;
    }

    if(QMessageBox::question(this, tr("Write Target"), prompt, QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
        return;

    dumpSetBusy(true);
    QString output;
    bool mifareUidFallbackOk = false;
    bool mifareRawBlock0Failed = false;
    if(dumpLastFamily == "EM410x" && dumpLastCardId.length() == 10)
        output = "--- lf em 410x clone ---\n" + simpleRunCommand("lf em 410x clone --id " + dumpLastCardId, 10000);
    else if(dumpLastFamily == "MIFARE Classic")
    {
        output = "--- hf mf restore ---\n" + simpleRunCommand("hf mf restore --" + dumpMifareCardArg() + " --force" + dumpMifareFileArgs(), 25000);
        QString block0 = dumpMifareBlockData(0);
        auto commandLooksOk = [](const QString& text) {
            QString lower = text.toLower();
            return !lower.contains("fail") &&
                   !lower.contains("error") &&
                   !lower.contains("can't") &&
                   !lower.contains("did not ack");
        };
        if(!block0.isEmpty())
        {
            QString block0Command = "hf mf csetblk --blk 0 -d " + block0;
            QString block0Output;
            output += "\n\n--- magic block 0 / UID block ---\n";
            block0Output = simpleRunCommand(block0Command, 8000);
            output += "--- " + block0Command + " ---\n" + block0Output;
            mifareRawBlock0Failed = block0Output.contains("fail", Qt::CaseInsensitive) ||
                                    block0Output.contains("error", Qt::CaseInsensitive);
            if(mifareRawBlock0Failed && (dumpLastCardId.length() == 8 || dumpLastCardId.length() == 14 || dumpLastCardId.length() == 20))
            {
                QString uidCommand = "hf mf csetuid --uid " + dumpLastCardId;
                QString atqa = simpleExtractHfParameter(dumpSourceOutput, "ATQA");
                QString sak = simpleExtractHfParameter(dumpSourceOutput, "SAK");
                if(!atqa.isEmpty())
                    uidCommand += " --atqa " + atqa;
                if(!sak.isEmpty())
                    uidCommand += " --sak " + sak;
                QString uidOutput = simpleRunCommand(uidCommand, 8000);
                mifareUidFallbackOk = commandLooksOk(uidOutput);
                output += "\n\n--- UID-card fallback ---\n";
                output += "--- " + uidCommand + " ---\n" + uidOutput;

                if(!mifareUidFallbackOk)
                {
                    QString wrblACommand = "hf mf wrbl --blk 0 -a -k FFFFFFFFFFFF -d " + block0 + " --force";
                    QString wrblAOutput = simpleRunCommand(wrblACommand, 8000);
                    mifareUidFallbackOk = commandLooksOk(wrblAOutput);
                    output += "\n\n--- CUID normal block-0 fallback / key A ---\n";
                    output += "--- " + wrblACommand + " ---\n" + wrblAOutput;
                }

                if(!mifareUidFallbackOk)
                {
                    QString wrblBCommand = "hf mf wrbl --blk 0 -b -k FFFFFFFFFFFF -d " + block0 + " --force";
                    QString wrblBOutput = simpleRunCommand(wrblBCommand, 8000);
                    mifareUidFallbackOk = commandLooksOk(wrblBOutput);
                    output += "\n\n--- CUID normal block-0 fallback / key B ---\n";
                    output += "--- " + wrblBCommand + " ---\n" + wrblBOutput;
                }

                if(!mifareUidFallbackOk)
                {
                    QString uscuid4Command = "script run hf_mf_uscuid_prog -t 4 -u " + dumpLastCardId;
                    QString uscuid4Output = simpleRunCommand(uscuid4Command, 10000);
                    mifareUidFallbackOk = commandLooksOk(uscuid4Output);
                    output += "\n\n--- USCUID script fallback / 40-43 wakeup ---\n";
                    output += "--- " + uscuid4Command + " ---\n" + uscuid4Output;
                }

                if(!mifareUidFallbackOk)
                {
                    QString uscuid2Command = "script run hf_mf_uscuid_prog -t 2 -u " + dumpLastCardId;
                    QString uscuid2Output = simpleRunCommand(uscuid2Command, 10000);
                    mifareUidFallbackOk = commandLooksOk(uscuid2Output);
                    output += "\n\n--- USCUID script fallback / 20-23 wakeup ---\n";
                    output += "--- " + uscuid2Command + " ---\n" + uscuid2Output;
                }
            }
        }
        else
        {
            output += "\n\n--- magic block 0 / UID block ---\n";
            output += "Source dump did not expose block 0 data, so block 0 was not written.\n";
        }
    }

    dumpWriteOutput = output;
    dumpOutputEdit->setPlainText((dumpReadOutput + "\n\n" + dumpWriteOutput).trimmed());
    if(output.toLower().contains("fail") || output.toLower().contains("error"))
    {
        if(dumpLastFamily == "MIFARE Classic" && mifareUidFallbackOk)
            dumpSetStatus(tr("Raw block 0 failed, but the UID-card fallback command finished. Run Verify to confirm the target ID and data."), "amber");
        else if(dumpLastFamily == "MIFARE Classic" && (mifareRawBlock0Failed || (output.contains("blk | data", Qt::CaseInsensitive) && output.contains(" 0 |"))))
            dumpSetStatus(tr("Most data was written, but block 0 failed. The target must be a compatible magic MIFARE card to clone UID/manufacturer block."), "amber");
        else
            dumpSetStatus(tr("Write reported an error. Check the output for the failed block/sector."), "red");
    }
    else
        dumpSetStatus(tr("Write command finished. Run Verify with the target card still on the reader."), "green");
    dumpSetBusy(false);
}

void MainWindow::dumpRunVerify()
{
    if(!pm3state)
    {
        dumpSetStatus(tr("Connect to the Proxmark3 first."), "amber");
        return;
    }

    dumpSetBusy(true);
    QString output;
    if(dumpLastFamily == "EM410x")
        output = "--- verify lf em 410x reader ---\n" + simpleRunCommand("lf em 410x reader", 6000);
    else if(dumpLastFamily == "MIFARE Classic")
        output = "--- verify hf mf dump ---\n" + simpleRunCommand("hf mf dump --" + dumpMifareCardArg() + dumpMifareFileArgs(), 25000);
    else if(simpleFrequencyForFamily(dumpLastFamily).startsWith("HF"))
        output = "--- verify hf search ---\n" + simpleRunCommand("hf search", 5000);
    else
        output = "--- verify lf search ---\n" + simpleRunCommand("lf search", 5000);

    dumpVerifyOutput = output;
    dumpTargetBlockMap = dumpLoadMifareBlockMapFromLatestFile();
    if(dumpTargetBlockMap.isEmpty())
        dumpTargetBlockMap = dumpExtractMifareBlockMap(dumpVerifyOutput);
    dumpOutputEdit->setPlainText((dumpReadOutput + "\n\n" + dumpWriteOutput + "\n\n" + dumpVerifyOutput).trimmed());
    dumpRefreshCardMap();

    QString verifyId = simpleExtractCardId(output, dumpLastFamily);
    if(dumpLastFamily == "EM410x")
    {
        if(!dumpLastCardId.isEmpty() && verifyId == dumpLastCardId)
            dumpSetStatus(tr("Verified. The target readable ID matches the original."), "green");
        else
            dumpSetStatus(tr("Verification failed. The target readable ID does not match the original."), "red");
    }
    else if(dumpLastFamily == "MIFARE Classic")
    {
        QMap<int, QString> sourceBlocks = dumpSourceBlockMap;
        if(sourceBlocks.isEmpty())
            sourceBlocks = dumpExtractMifareBlockMap(dumpReadOutput);
        QMap<int, QString> targetBlocks = dumpTargetBlockMap;
        if(targetBlocks.isEmpty())
            targetBlocks = dumpExtractMifareBlockMap(dumpVerifyOutput);
        if(!sourceBlocks.isEmpty() && sourceBlocks == targetBlocks)
            dumpSetStatus(tr("Verified. Parsed block data from target matches the source dump output."), "green");
        else if(sourceBlocks.isEmpty() || targetBlocks.isEmpty())
            dumpSetStatus(tr("Verify finished, but the client output did not expose comparable block data. Inspect the output for failed sectors."), "amber");
        else if(sourceBlocks.value(0) != targetBlocks.value(0))
            dumpSetStatus(tr("Verification mismatch: block 0 / UID differs. The target did not accept the magic UID/manufacturer-block write."), "red");
        else
            dumpSetStatus(tr("Verification mismatch. One or more parsed blocks differ from the source dump."), "red");
    }
    else
    {
        dumpSetStatus(tr("Verify scan finished. Inspect the output for this unsupported family."), "amber");
    }

    dumpSetBusy(false);
}

void MainWindow::dumpRunCleanFiles()
{
    QString uid = dumpLastCardId.toUpper();
    uid.remove(QRegularExpression("[^0-9A-F]"));
    if(uid.isEmpty())
    {
        dumpSetStatus(tr("Detect a card first, then cleanup can target that UID."), "amber");
        return;
    }

    QDir fileDir = dumpGeneratedFilesDir();
    QFileInfoList files = dumpGeneratedFiles(uid);

    if(files.isEmpty())
    {
        dumpSetStatus(tr("No generated files found for UID %1.").arg(uid), "amber");
        return;
    }

    if(QMessageBox::question(this, tr("Clean Generated Files"),
                             tr("Delete %1 generated file(s) for UID %2 from:\n%3")
                             .arg(files.size())
                             .arg(uid)
                             .arg(QDir::toNativeSeparators(fileDir.absolutePath())),
                             QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
        return;

    int removed = 0;
    QStringList failed;
    for(const QFileInfo& file : files)
    {
        if(QFile::remove(file.absoluteFilePath()))
            removed++;
        else
            failed << file.fileName();
    }

    if(failed.isEmpty())
        dumpSetStatus(tr("Removed %1 generated file(s) for UID %2.").arg(removed).arg(uid), "green");
    else
        dumpSetStatus(tr("Removed %1 file(s), but could not delete: %2").arg(removed).arg(failed.join(", ")), "red");
}

QDir MainWindow::dumpGeneratedFilesDir() const
{
    if(clientWorkingDir != nullptr && clientWorkingDir->exists())
        return *clientWorkingDir;

    QString clientPath = findModernClientPath();
    if(!clientPath.isEmpty())
        return QFileInfo(clientPath).absoluteDir();

    return QDir(QApplication::applicationDirPath());
}

QFileInfoList MainWindow::dumpGeneratedFiles(const QString& uid) const
{
    QString normalizedUid = uid.toUpper();
    normalizedUid.remove(QRegularExpression("[^0-9A-F]"));

    QStringList patterns;
    if(normalizedUid.isEmpty())
        patterns << "hf-mf-*";
    else
        patterns << "hf-mf-" + normalizedUid + "-*";

    QList<QDir> dirs = {
        dumpGeneratedFilesDir(),
        QDir(QApplication::applicationDirPath()),
        QDir(QDir::currentPath()),
    };

    QFileInfoList files;
    QSet<QString> seen;
    for(const QDir& dir : dirs)
    {
        QFileInfoList localFiles = dir.entryInfoList(patterns, QDir::Files, QDir::Name);
        for(const QFileInfo& file : localFiles)
        {
            QString path = QDir::cleanPath(file.absoluteFilePath()).toLower();
            if(seen.contains(path))
                continue;
            seen.insert(path);
            files.append(file);
        }
    }
    return files;
}

bool MainWindow::dumpMirrorMifareKeyFile()
{
    QString keyFile = dumpMifareKeyFilename();
    if(keyFile.isEmpty())
        return false;

    QList<QDir> dirs = {
        dumpGeneratedFilesDir(),
        QDir(QApplication::applicationDirPath()),
        QDir(QDir::currentPath()),
    };

    QString sourcePath;
    for(const QDir& dir : dirs)
    {
        QString candidate = dir.absoluteFilePath(keyFile);
        if(QFileInfo::exists(candidate))
        {
            sourcePath = candidate;
            break;
        }
    }
    if(sourcePath.isEmpty())
        return false;

    bool copied = false;
    QSet<QString> targets;
    for(const QDir& dir : dirs)
    {
        QString targetPath = QDir::cleanPath(dir.absoluteFilePath(keyFile));
        QString normalizedTarget = targetPath.toLower();
        if(targets.contains(normalizedTarget))
            continue;
        targets.insert(normalizedTarget);

        if(QDir::cleanPath(sourcePath).compare(targetPath, Qt::CaseInsensitive) == 0)
            continue;
        QFile::remove(targetPath);
        copied = QFile::copy(sourcePath, targetPath) || copied;
    }
    return copied;
}

void MainWindow::dumpApplyDetectOutput(const QString& output)
{
    dumpSourceOutput = output;
    dumpLastFamily = simpleDetectFamily(output);
    dumpLastCardId = simpleExtractCardId(output, dumpLastFamily);
    if(dumpLastFamily == "LF tag" && output.toLower().contains("em"))
        dumpLastFamily = "EM410x";

    if(dumpFamilyLabel != nullptr)
        dumpFamilyLabel->setText(dumpLastFamily);
    if(dumpFrequencyLabel != nullptr)
        dumpFrequencyLabel->setText(simpleFrequencyForFamily(dumpLastFamily).isEmpty() ? tr("Unknown") : simpleFrequencyForFamily(dumpLastFamily));
    if(dumpIdLabel != nullptr)
        dumpIdLabel->setText(dumpLastCardId.isEmpty() ? tr("Not found") : dumpLastCardId);
    if(dumpCapabilityLabel != nullptr)
        dumpCapabilityLabel->setText(dumpCapabilityForFamily(dumpLastFamily));
    if(dumpOutputEdit != nullptr)
        dumpOutputEdit->setPlainText(output.trimmed());

    dumpRefreshFields();
    if(dumpLastFamily == "Unknown")
        dumpSetStatus(tr("No supported card was recognized. Try a different mode or reposition the card."), "amber");
    else
        dumpSetStatus(tr("Card detected. You can run Dump / Read next."), "green");
}

void MainWindow::dumpRefreshFields()
{
    if(dumpFieldTable == nullptr)
        return;

    QList<QPair<QString, QString>> rows;
    rows.append(qMakePair(tr("Family"), dumpLastFamily.isEmpty() ? tr("Unknown") : dumpLastFamily));
    rows.append(qMakePair(tr("Frequency"), simpleFrequencyForFamily(dumpLastFamily).isEmpty() ? tr("Unknown") : simpleFrequencyForFamily(dumpLastFamily)));
    rows.append(qMakePair(tr("Visible ID"), dumpLastCardId.isEmpty() ? tr("Not found") : dumpLastCardId));

    const QString combined = dumpSourceOutput + "\n" + dumpReadOutput;
    QString atqa = simpleExtractHfParameter(combined, "ATQA");
    QString sak = simpleExtractHfParameter(combined, "SAK");
    if(!atqa.isEmpty())
        rows.append(qMakePair(tr("ATQA"), atqa));
    if(!sak.isEmpty())
        rows.append(qMakePair(tr("SAK"), sak));

    QMap<int, QString> blocks = dumpSourceBlockMap;
    if(blocks.isEmpty())
        blocks = dumpExtractMifareBlockMap(dumpReadOutput);
    if(!blocks.isEmpty())
        rows.append(qMakePair(tr("Parsed data blocks"), QString::number(blocks.size())));

    dumpFieldTable->setRowCount(rows.size());
    for(int i = 0; i < rows.size(); i++)
    {
        dumpFieldTable->setItem(i, 0, new QTableWidgetItem(rows[i].first));
        dumpFieldTable->setItem(i, 1, new QTableWidgetItem(rows[i].second));
    }
    dumpFieldTable->resizeColumnToContents(0);
    dumpRefreshCardMap();
}

void MainWindow::dumpRefreshCardMap()
{
    if(dumpMapTable == nullptr)
        return;

    dumpMapTable->setRowCount(0);

    if(dumpLastFamily != "MIFARE Classic")
        return;

    QMap<int, QString> sourceBlocks = dumpSourceBlockMap;
    if(sourceBlocks.isEmpty())
        sourceBlocks = dumpExtractMifareBlockMap(dumpReadOutput);
    QMap<int, QString> targetBlocks = dumpTargetBlockMap;
    if(targetBlocks.isEmpty())
        targetBlocks = dumpExtractMifareBlockMap(dumpVerifyOutput);
    if(sourceBlocks.isEmpty())
        return;

    auto formatBlock = [](const QString& block) {
        QStringList bytes;
        for(int i = 0; i + 1 < block.length(); i += 2)
            bytes << block.mid(i, 2);
        return bytes.join(' ');
    };

    auto makeItem = [](const QString& text, const QColor& background, const QColor& foreground = QColor()) {
        QTableWidgetItem* item = new QTableWidgetItem(text);
        item->setBackground(background);
        if(foreground.isValid())
            item->setForeground(foreground);
        return item;
    };

    const QColor uidColor(55, 93, 125);
    const QColor dataColor(19, 31, 38);
    const QColor trailerColor(74, 55, 30);
    const QColor okColor(22, 70, 42);
    const QColor diffColor(92, 36, 36);
    const QColor pendingColor(54, 58, 62);
    const QColor textColor(238, 241, 243);

    int row = 0;
    for(auto it = sourceBlocks.constBegin(); it != sourceBlocks.constEnd(); ++it)
    {
        int block = it.key();
        int sector = block / 4;
        QString role = tr("Data");
        QColor roleColor = dataColor;
        if(block == 0)
        {
            role = tr("UID / manufacturer");
            roleColor = uidColor;
        }
        else if(block % 4 == 3)
        {
            role = tr("Sector trailer / keys");
            roleColor = trailerColor;
        }

        QString target = targetBlocks.value(block);
        QString status;
        QColor statusColor = pendingColor;
        if(targetBlocks.isEmpty())
        {
            status = tr("Source");
        }
        else if(target.isEmpty())
        {
            status = tr("Missing");
            statusColor = diffColor;
        }
        else if(target == it.value())
        {
            status = tr("Match");
            statusColor = okColor;
        }
        else
        {
            status = block == 0 ? tr("UID differs") : tr("Differs");
            statusColor = diffColor;
        }

        dumpMapTable->insertRow(row);
        dumpMapTable->setItem(row, 0, makeItem(QString::number(sector), roleColor, textColor));
        dumpMapTable->setItem(row, 1, makeItem(QString::number(block), roleColor, textColor));
        dumpMapTable->setItem(row, 2, makeItem(role, roleColor, textColor));
        dumpMapTable->setItem(row, 3, makeItem(formatBlock(it.value()), roleColor, textColor));
        dumpMapTable->setItem(row, 4, makeItem(target.isEmpty() ? "-" : formatBlock(target), target.isEmpty() ? pendingColor : roleColor, textColor));
        dumpMapTable->setItem(row, 5, makeItem(status, statusColor, textColor));
        row++;
    }
}

QString MainWindow::dumpMifareCardArg() const
{
    if(dumpMifareSizeBox == nullptr)
        return "1k";
    QString value = dumpMifareSizeBox->currentData().toString();
    return value.isEmpty() ? "1k" : value;
}

QString MainWindow::dumpMifareKeyFilename() const
{
    QString uid = dumpLastCardId.toUpper();
    uid.remove(QRegularExpression("[^0-9A-F]"));
    if(uid.isEmpty())
        return "";
    return "hf-mf-" + uid + "-key.bin";
}

QString MainWindow::dumpMifareDataFilename() const
{
    QString uid = dumpLastCardId.toUpper();
    uid.remove(QRegularExpression("[^0-9A-F]"));
    if(uid.isEmpty())
        return "";
    return "hf-mf-" + uid + "-dump.bin";
}

QString MainWindow::dumpMifareFileArgs() const
{
    QString args;
    QString dataFile = dumpMifareDataFilename();
    if(!dataFile.isEmpty())
    {
        dataFile.chop(4);
        args += " -f " + dataFile;
    }
    return args;
}

QString MainWindow::dumpCapabilityForFamily(const QString& family) const
{
    if(family == "EM410x")
        return tr("Can copy the visible LF ID to a writable T5577-compatible target and verify the ID.");
    if(family == "MIFARE Classic")
        return tr("Can run full dump/restore. A true 1:1 result requires readable sectors/keys and a compatible magic target card.");
    if(family == "NTAG")
        return tr("Can read/dump pages for inspection. Automatic restore is not enabled yet because locks, counters, and passwords vary.");
    if(family == "ISO14443-A / NFC")
        return tr("Can show public ISO14443-A details. Full data copy is not mapped for this chip family.");
    if(family == "HID Prox" || family == "LF tag" || family == "T55xx")
        return tr("Can inspect LF output. Automatic duplicate flow is currently mapped only for EM410x ID tags.");
    return tr("Unsupported or not recognized yet.");
}

bool MainWindow::dumpCanWriteFamily(const QString& family) const
{
    return family == "EM410x" || family == "MIFARE Classic";
}

QString MainWindow::dumpMifareBlockData(int block) const
{
    QString text = dumpReadOutput;
    QRegularExpression linePattern(QString("^\\s*(?:\\[.?\\]\\s*)?%1\\s*\\|\\s*((?:[0-9A-Fa-f]{2}\\s+){15}[0-9A-Fa-f]{2})\\s*\\|")
                                   .arg(block),
                                   QRegularExpression::MultilineOption);
    QRegularExpressionMatch match = linePattern.match(text);
    if(!match.hasMatch())
        return "";

    QString data = match.captured(1).toUpper();
    data.remove(QRegularExpression("[^0-9A-F]"));
    return data.length() == 32 ? data : "";
}

QMap<int, QString> MainWindow::dumpExtractMifareBlockMap(const QString& output) const
{
    QMap<int, QString> blocks;
    QRegularExpression linePattern("^\\s*(?:\\[.?\\]\\s*)?(\\d{1,3})\\s*\\|\\s*((?:[0-9A-Fa-f]{2}\\s+){15}[0-9A-Fa-f]{2})\\s*\\|",
                                   QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = linePattern.globalMatch(output);
    while(it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        bool ok = false;
        int block = match.captured(1).toInt(&ok);
        if(!ok)
            continue;

        QString data = match.captured(2).toUpper();
        data.remove(QRegularExpression("[^0-9A-F]"));
        if(data.length() == 32)
            blocks.insert(block, data);
    }
    return blocks;
}

QMap<int, QString> MainWindow::dumpLoadMifareBlockMapFromLatestFile() const
{
    QMap<int, QString> blocks;
    if(dumpLastCardId.isEmpty())
        return blocks;

    QFileInfoList files = dumpGeneratedFiles(dumpLastCardId);
    QFileInfo latestJson;
    QFileInfo latestBin;
    for(const QFileInfo& file : files)
    {
        QString name = file.fileName().toLower();
        if(!name.contains("-dump"))
            continue;
        if(name.endsWith(".json") && (!latestJson.exists() || file.lastModified() > latestJson.lastModified()))
            latestJson = file;
        else if(name.endsWith(".bin") && (!latestBin.exists() || file.lastModified() > latestBin.lastModified()))
            latestBin = file;
    }

    if(latestJson.exists())
    {
        QFile file(latestJson.absoluteFilePath());
        if(file.open(QIODevice::ReadOnly))
        {
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
            if(error.error == QJsonParseError::NoError && doc.isObject())
            {
                QJsonObject root = doc.object();
                QJsonObject jsonBlocks = root.value("blocks").toObject();
                for(auto it = jsonBlocks.constBegin(); it != jsonBlocks.constEnd(); ++it)
                {
                    bool ok = false;
                    int block = it.key().toInt(&ok);
                    QString data = it.value().toString().toUpper();
                    data.remove(QRegularExpression("[^0-9A-F]"));
                    if(ok && data.length() == 32)
                        blocks.insert(block, data);
                }
            }
        }
        if(!blocks.isEmpty())
            return blocks;
    }

    if(latestBin.exists())
    {
        QFile file(latestBin.absoluteFilePath());
        if(file.open(QIODevice::ReadOnly))
        {
            QByteArray bytes = file.readAll();
            int blockCount = bytes.size() / 16;
            for(int block = 0; block < blockCount; block++)
            {
                QByteArray blockBytes = bytes.mid(block * 16, 16);
                blocks.insert(block, QString::fromLatin1(blockBytes.toHex().toUpper()));
            }
        }
    }

    return blocks;
}

QStringList MainWindow::dumpExtractHexBlocks(const QString& output) const
{
    QStringList blocks;
    QRegularExpression blockPattern("\\b((?:[0-9A-Fa-f]{2}\\s+){15}[0-9A-Fa-f]{2})\\b");
    QRegularExpressionMatchIterator it = blockPattern.globalMatch(output);
    while(it.hasNext())
    {
        QString block = it.next().captured(1).toUpper();
        block.remove(QRegularExpression("[^0-9A-F]"));
        if(block.length() == 32 && !blocks.contains(block))
            blocks.append(block);
    }
    return blocks;
}

void MainWindow::makeTabScrollable(QWidget* tab)
{
    if(tab == nullptr || tab->property("scrollWrapped").toBool())
        return;

    QLayout* originalLayout = tab->layout();
    if(originalLayout == nullptr)
        return;

    QWidget* content = new QWidget(tab);
    content->setObjectName(tab->objectName() + "_scrollContent");
    QVBoxLayout* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(5);
    while(QLayoutItem* item = originalLayout->takeAt(0))
    {
        if(QWidget* widget = item->widget())
        {
            widget->setParent(content);
            contentLayout->addWidget(widget);
            delete item;
        }
        else if(QLayout* layout = item->layout())
        {
            contentLayout->addLayout(layout);
            delete item;
        }
        else
        {
            contentLayout->addItem(item);
        }
    }

    delete originalLayout;

    QScrollArea* scrollArea = new QScrollArea(tab);
    scrollArea->setObjectName(tab->objectName() + "_scrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(content);

    QVBoxLayout* wrapperLayout = new QVBoxLayout(tab);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->addWidget(scrollArea);
    tab->setProperty("scrollWrapped", true);
}

void MainWindow::normalizeAdvancedPage(QWidget* tab)
{
    if(tab == nullptr)
        return;

    tab->setVisible(true);
    tab->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    for(QScrollArea* scrollArea : tab->findChildren<QScrollArea*>())
    {
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        if(scrollArea->widget() != nullptr)
        {
            scrollArea->widget()->setVisible(true);
            scrollArea->widget()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
            if(tab == ui->lfTab)
                scrollArea->widget()->setMinimumWidth(760);
            else if(tab == ui->mifareTab)
                scrollArea->widget()->setMinimumWidth(980);
        }
    }

    for(QGroupBox* box : tab->findChildren<QGroupBox*>())
    {
        box->setVisible(true);
        box->setMaximumHeight(16777215);
        if(box->isCheckable())
            box->setChecked(true);
    }

    if(tab == ui->lfTab)
    {
        QList<QLayout*> lfLayouts = {
            ui->LF_LFconfigGroupBox->layout(),
            ui->LF_LFConf_freqGroupBox->layout(),
            ui->LF_operationGroupBox->layout(),
        };
        for(QLayout* layout : lfLayouts)
        {
            if(layout == nullptr)
                continue;
            layout->setSizeConstraint(QLayout::SetMinimumSize);
            layout->setSpacing(qMax(layout->spacing(), 10));
            layout->setContentsMargins(12, 14, 12, 12);
        }

        ui->LF_LFconfigGroupBox->setMinimumWidth(720);
        ui->LF_LFconfigGroupBox->setMinimumHeight(430);
        ui->LF_LFConf_freqGroupBox->setMinimumHeight(225);
        ui->LF_LFConf_freqGroupBox->setFixedHeight(225);
        ui->LF_operationGroupBox->setMinimumHeight(255);
        ui->LF_LFConf_freqSlider->setMinimumHeight(42);
        ui->LF_LFConf_freqLabel->setMinimumWidth(170);
        if(QLabel* hintLabel = ui->lfTab->findChild<QLabel*>("label_22"))
        {
            hintLabel->setMinimumHeight(70);
            hintLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
            hintLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        }
    }
    else if(tab == ui->mifareTab)
    {
        if(tab->property("mifareModernLayout").toBool())
            return;
        tab->setProperty("mifareModernLayout", true);

        auto resetLayout = [](QWidget* widget)
        {
            if(widget == nullptr || widget->layout() == nullptr)
                return;

            const QList<QWidget*> directChildren = widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
            for(QWidget* child : directChildren)
                child->hide();

            QLayout* layout = widget->layout();
            while(QLayoutItem* item = layout->takeAt(0))
            {
                delete item;
            }
            delete layout;
        };

        auto showInLayout = [](QWidget* widget, QLayout* layout)
        {
            if(widget == nullptr || layout == nullptr)
                return;
            widget->show();
            layout->addWidget(widget);
        };

        auto makeStaticBox = [](QGroupBox* box)
        {
            if(box == nullptr)
                return;
            box->setCheckable(false);
            box->setChecked(false);
            box->setMaximumHeight(16777215);
            box->show();
        };

        QScrollArea* scrollArea = tab->findChild<QScrollArea*>();
        if(scrollArea == nullptr)
            return;

        QWidget* content = new QWidget(scrollArea);
        content->setObjectName("mifareModernContent");
        QVBoxLayout* pageLayout = new QVBoxLayout(content);
        pageLayout->setContentsMargins(14, 14, 14, 14);
        pageLayout->setSpacing(12);

        QHBoxLayout* tableLayout = new QHBoxLayout();
        tableLayout->setSpacing(12);

        QGroupBox* dataBox = new QGroupBox(tr("Card Data"), content);
        QVBoxLayout* dataLayout = new QVBoxLayout(dataBox);
        dataBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        ui->MF_dataWidget->setParent(dataBox);
        ui->MF_dataWidget->setMinimumSize(520, 300);
        ui->MF_dataWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        ui->MF_dataWidget->show();
        ui->MF_dataWidget->horizontalHeader()->setStretchLastSection(true);
        ui->MF_dataWidget->verticalHeader()->setDefaultSectionSize(22);
        dataLayout->addWidget(ui->MF_dataWidget);

        QGroupBox* selectionBox = new QGroupBox(tr("Selection"), content);
        QVBoxLayout* selectionLayout = new QVBoxLayout(selectionBox);
        selectionLayout->setSpacing(8);
        selectionBox->setMinimumWidth(170);
        QList<QWidget*> selectionWidgets = {
            ui->MF_selectAllBox,
            ui->MF_selectTrailerBox,
            ui->MF_data2KeyButton,
            ui->MF_key2DataButton,
            ui->MF_fillKeysButton,
            ui->MF_trailerDecoderButton,
        };
        for(QWidget* widget : selectionWidgets)
        {
            if(widget == nullptr)
                continue;
            widget->setParent(selectionBox);
            showInLayout(widget, selectionLayout);
        }
        selectionLayout->addStretch(1);

        QGroupBox* keysBox = new QGroupBox(tr("Keys"), content);
        QVBoxLayout* keysLayout = new QVBoxLayout(keysBox);
        keysBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        ui->MF_keyWidget->setParent(keysBox);
        ui->MF_keyWidget->setMinimumSize(330, 300);
        ui->MF_keyWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        ui->MF_keyWidget->show();
        ui->MF_keyWidget->horizontalHeader()->setStretchLastSection(true);
        ui->MF_keyWidget->verticalHeader()->setDefaultSectionSize(22);
        keysLayout->addWidget(ui->MF_keyWidget);

        tableLayout->addWidget(dataBox, 3);
        tableLayout->addWidget(selectionBox);
        tableLayout->addWidget(keysBox, 2);
        pageLayout->addLayout(tableLayout, 2);

        QHBoxLayout* utilityLayout = new QHBoxLayout();
        utilityLayout->setSpacing(12);
        resetLayout(ui->MF_typeGroupBox);
        makeStaticBox(ui->MF_typeGroupBox);
        QHBoxLayout* typeLayout = new QHBoxLayout(ui->MF_typeGroupBox);
        typeLayout->setContentsMargins(12, 18, 12, 12);
        typeLayout->setSpacing(12);
        QList<QWidget*> typeWidgets = {
            ui->MF_Type_miniButton,
            ui->MF_Type_1kButton,
            ui->MF_Type_2kButton,
            ui->MF_Type_4kButton,
        };
        for(QWidget* widget : typeWidgets)
        {
            widget->setParent(ui->MF_typeGroupBox);
            showInLayout(widget, typeLayout);
        }

        resetLayout(ui->MF_fileGroupBox);
        makeStaticBox(ui->MF_fileGroupBox);
        QHBoxLayout* fileLayout = new QHBoxLayout(ui->MF_fileGroupBox);
        fileLayout->setContentsMargins(12, 18, 12, 12);
        fileLayout->setSpacing(8);
        QList<QWidget*> fileWidgets = {
            ui->MF_File_loadButton,
            ui->MF_File_saveButton,
            ui->MF_File_clearButton,
            ui->MF_File_dataButton,
            ui->MF_File_keyButton,
        };
        for(QWidget* widget : fileWidgets)
        {
            widget->setParent(ui->MF_fileGroupBox);
            showInLayout(widget, fileLayout);
        }

        resetLayout(ui->MF_attackGroupBox);
        makeStaticBox(ui->MF_attackGroupBox);
        QHBoxLayout* attackLayout = new QHBoxLayout(ui->MF_attackGroupBox);
        attackLayout->setContentsMargins(12, 18, 12, 12);
        attackLayout->setSpacing(8);
        QList<QWidget*> attackWidgets = {
            ui->MF_Attack_infoButton,
            ui->MF_Attack_chkButton,
            ui->MF_Attack_nestedButton,
            ui->MF_Attack_hardnestedButton,
            ui->MF_Attack_darksideButton,
        };
        for(QWidget* widget : attackWidgets)
        {
            widget->setParent(ui->MF_attackGroupBox);
            showInLayout(widget, attackLayout);
        }

        QList<QGroupBox*> utilityBoxes = { ui->MF_typeGroupBox, ui->MF_fileGroupBox, ui->MF_attackGroupBox };
        for(QGroupBox* box : utilityBoxes)
        {
            box->setParent(content);
            box->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            utilityLayout->addWidget(box);
        }
        utilityLayout->addStretch(1);
        pageLayout->addLayout(utilityLayout);

        resetLayout(ui->MF_normalGroupBox);
        makeStaticBox(ui->MF_normalGroupBox);
        QGridLayout* normalLayout = new QGridLayout(ui->MF_normalGroupBox);
        normalLayout->setSpacing(8);
        ui->MF_RW_readSelectedButton->setParent(ui->MF_normalGroupBox);
        ui->MF_RW_dumpButton->setParent(ui->MF_normalGroupBox);
        ui->MF_RW_writeSelectedButton->setParent(ui->MF_normalGroupBox);
        ui->MF_RW_restoreButton->setParent(ui->MF_normalGroupBox);
        ui->MF_RW_readBlockButton->setParent(ui->MF_normalGroupBox);
        ui->MF_RW_writeBlockButton->setParent(ui->MF_normalGroupBox);
        QList<QWidget*> normalButtons = {
            ui->MF_RW_readSelectedButton,
            ui->MF_RW_dumpButton,
            ui->MF_RW_writeSelectedButton,
            ui->MF_RW_restoreButton,
            ui->MF_RW_readBlockButton,
            ui->MF_RW_writeBlockButton,
        };
        for(QWidget* widget : normalButtons)
            widget->show();
        normalLayout->addWidget(ui->MF_RW_readSelectedButton, 0, 0);
        normalLayout->addWidget(ui->MF_RW_dumpButton, 0, 1);
        normalLayout->addWidget(ui->MF_RW_writeSelectedButton, 1, 0);
        normalLayout->addWidget(ui->MF_RW_restoreButton, 1, 1);
        normalLayout->addWidget(ui->MF_RW_readBlockButton, 2, 0);
        normalLayout->addWidget(ui->MF_RW_writeBlockButton, 2, 1);

        resetLayout(ui->MF_UIDGroupBox);
        makeStaticBox(ui->MF_UIDGroupBox);
        QGridLayout* uidLayout = new QGridLayout(ui->MF_UIDGroupBox);
        uidLayout->setSpacing(8);
        QList<QPushButton*> uidButtons = {
            ui->MF_UID_readSelectedButton,
            ui->MF_UID_setParaButton,
            ui->MF_UID_lockButton,
            ui->MF_UID_writeSelectedButton,
            ui->MF_UID_wipeButton,
            ui->MF_UID_aboutUIDButton,
            ui->MF_UID_readBlockButton,
            ui->MF_UID_writeBlockButton,
        };
        for(QPushButton* button : uidButtons)
        {
            button->setParent(ui->MF_UIDGroupBox);
            button->show();
        }
        uidLayout->addWidget(ui->MF_UID_readSelectedButton, 0, 0);
        uidLayout->addWidget(ui->MF_UID_setParaButton, 0, 1);
        uidLayout->addWidget(ui->MF_UID_lockButton, 0, 2);
        uidLayout->addWidget(ui->MF_UID_writeSelectedButton, 1, 0);
        uidLayout->addWidget(ui->MF_UID_wipeButton, 1, 1);
        uidLayout->addWidget(ui->MF_UID_aboutUIDButton, 1, 2);
        uidLayout->addWidget(ui->MF_UID_readBlockButton, 2, 0);
        uidLayout->addWidget(ui->MF_UID_writeBlockButton, 2, 1);

        resetLayout(ui->MF_RWGroupBox);
        makeStaticBox(ui->MF_RWGroupBox);
        QGridLayout* rwLayout = new QGridLayout(ui->MF_RWGroupBox);
        rwLayout->setContentsMargins(12, 18, 12, 12);
        rwLayout->setHorizontalSpacing(16);
        rwLayout->setVerticalSpacing(10);

        QGroupBox* singleBlockBox = new QGroupBox(tr("Single Block"), ui->MF_RWGroupBox);
        QGridLayout* singleLayout = new QGridLayout(singleBlockBox);
        singleLayout->setSpacing(8);
        ui->MF_RW_blockBox->setParent(singleBlockBox);
        ui->MF_RW_keyEdit->setParent(singleBlockBox);
        ui->MF_RW_keyTypeBox->setParent(singleBlockBox);
        ui->MF_RW_dataEdit->setParent(singleBlockBox);
        ui->MF_RW_dataEdit->setMinimumHeight(30);
        ui->MF_RW_dataEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        ui->MF_RW_blockBox->show();
        ui->MF_RW_keyEdit->show();
        ui->MF_RW_keyTypeBox->show();
        ui->MF_RW_dataEdit->show();
        singleLayout->addWidget(new QLabel(tr("Block:"), singleBlockBox), 0, 0);
        singleLayout->addWidget(ui->MF_RW_blockBox, 0, 1);
        singleLayout->addWidget(new QLabel(tr("Key:"), singleBlockBox), 1, 0);
        singleLayout->addWidget(ui->MF_RW_keyEdit, 1, 1);
        singleLayout->addWidget(new QLabel(tr("Key Type:"), singleBlockBox), 2, 0);
        singleLayout->addWidget(ui->MF_RW_keyTypeBox, 2, 1);
        singleLayout->addWidget(new QLabel(tr("Data:"), singleBlockBox), 3, 0);
        singleLayout->addWidget(ui->MF_RW_dataEdit, 3, 1);

        ui->MF_normalGroupBox->setParent(ui->MF_RWGroupBox);
        ui->MF_UIDGroupBox->setParent(ui->MF_RWGroupBox);
        ui->MF_normalGroupBox->setMinimumWidth(280);
        ui->MF_UIDGroupBox->setMinimumWidth(430);
        rwLayout->addWidget(singleBlockBox, 0, 0);
        rwLayout->addWidget(ui->MF_normalGroupBox, 0, 1);
        rwLayout->addWidget(ui->MF_UIDGroupBox, 0, 2);
        rwLayout->setColumnStretch(0, 1);
        rwLayout->setColumnStretch(1, 1);
        rwLayout->setColumnStretch(2, 2);
        ui->MF_RWGroupBox->setParent(content);
        ui->MF_RWGroupBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        pageLayout->addWidget(ui->MF_RWGroupBox);

        QHBoxLayout* bottomLayout = new QHBoxLayout();
        bottomLayout->setSpacing(12);
        resetLayout(ui->MF_simGroupBox);
        makeStaticBox(ui->MF_simGroupBox);
        QHBoxLayout* simLayout = new QHBoxLayout(ui->MF_simGroupBox);
        simLayout->setContentsMargins(12, 18, 12, 12);
        simLayout->setSpacing(8);
        QList<QWidget*> simWidgets = {
            ui->MF_Sim_writeSelectedButton,
            ui->MF_Sim_readSelectedButton,
            ui->MF_Sim_clearButton,
            ui->MF_Sim_simButton,
        };
        for(QWidget* widget : simWidgets)
        {
            widget->setParent(ui->MF_simGroupBox);
            showInLayout(widget, simLayout);
        }

        resetLayout(ui->MF_sniffGroupBox);
        makeStaticBox(ui->MF_sniffGroupBox);
        QHBoxLayout* sniffLayout = new QHBoxLayout(ui->MF_sniffGroupBox);
        sniffLayout->setContentsMargins(12, 18, 12, 12);
        sniffLayout->setSpacing(8);
        QList<QWidget*> sniffWidgets = {
            ui->MF_Sniff_sniffButton,
            ui->MF_14aSniff_snoopButton,
            ui->MF_Sniff_listButton,
            ui->MF_Sniff_loadButton,
            ui->MF_Sniff_saveButton,
        };
        for(QWidget* widget : sniffWidgets)
        {
            widget->setParent(ui->MF_sniffGroupBox);
            showInLayout(widget, sniffLayout);
        }

        ui->MF_simGroupBox->setParent(content);
        ui->MF_sniffGroupBox->setParent(content);
        ui->MF_simGroupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        ui->MF_sniffGroupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        bottomLayout->addWidget(ui->MF_simGroupBox);
        bottomLayout->addWidget(ui->MF_sniffGroupBox);
        bottomLayout->addStretch(1);
        pageLayout->addLayout(bottomLayout);
        pageLayout->addStretch(1);

        content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
        content->setMinimumWidth(1120);
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setWidget(content);
    }
}

void MainWindow::loadClientPathList()
{
    m_clientPathList.clear();
    settings->beginGroup("Client_Path");
    int len = settings->beginReadArray("pathList");
    settings->endArray();
    if(settings->contains("path") && len == 0)
    {
        qDebug() << "Using old client path storage";
        m_clientPathList += settings->value("path", "proxmark3").toString();
    }
    else
    {
        int arrayLen = settings->beginReadArray("pathList");
        for(int i = 0; i < arrayLen; i++)
        {
            settings->setArrayIndex(i);
            QString path = settings->value("path").toString();
            if(!path.isEmpty())
                m_clientPathList += path;
        }
        settings->endArray();
    }
    settings->endGroup();

    QString modernClient = findModernClientPath();
    if(!modernClient.isEmpty() && !m_clientPathList.contains(modernClient))
        m_clientPathList.prepend(modernClient);

    ui->PM3_pathBox->clear();
    for(const QString& clientPath : qAsConst(m_clientPathList))
        ui->PM3_pathBox->addItem(clientPath);
}

void MainWindow::saveClientPathList()
{
    settings->beginGroup("Client_Path");
    if(settings->contains("path"))
    {
        qDebug() << "Upgrading client path storage";
        QString oldPath = settings->value("path").toString();
        if(!oldPath.isEmpty() && !m_clientPathList.contains(oldPath))
            m_clientPathList.append(oldPath);
        settings->remove("path");
    }

    settings->beginWriteArray("pathList");
    for(int i = 0; i < m_clientPathList.size(); i++)
    {
        settings->setArrayIndex(i);
        settings->setValue("path", m_clientPathList[i]);
    }
    settings->endArray();
    settings->endGroup();
}
// ***********************************************



void MainWindow::on_MF_Attack_darksideButton_clicked()
{
    setState(false);
    mifare->darkside();
    setState(true);
}

void MainWindow::on_Set_Client_startArgsEdit_editingFinished()
{
    settings->beginGroup("Client_Args");
    settings->setValue("args", ui->Set_Client_startArgsEdit->text());
    settings->endGroup();
}

void MainWindow::on_Set_Client_forceEnabledBox_stateChanged(int arg1)
{
    settings->beginGroup("Client_forceButtonsEnabled");
    keepButtonsEnabled = (arg1 == Qt::Checked);
    settings->setValue("state", keepButtonsEnabled);
    settings->endGroup();
    if(keepButtonsEnabled)
        setButtonsEnabled(true);
}

void MainWindow::on_Set_UI_setLanguageButton_clicked()
{
    QMessageBox::information(this, tr("Language"), tr("English is the only bundled language for now."));
}

void MainWindow::on_PM3_refreshPortButton_clicked()
{
    on_portSearchTimer_timeout();
    QStringList displayPorts;
    for(int i = 0; i < simplePortBox->count(); i++)
        displayPorts << simplePortBox->itemText(i);

    if(displayPorts.isEmpty())
        simpleSetFirmwareStatus(tr("No COM ports found. Plug in the device and try Refresh again."), "amber");
    else
        simpleSetFirmwareStatus(tr("Ports found: %1").arg(displayPorts.join(", ")), "green");
}

void MainWindow::on_Set_Client_envScriptEdit_editingFinished()
{
    settings->beginGroup("Client_Env");
    settings->setValue("scriptPath", ui->Set_Client_envScriptEdit->text());
    settings->endGroup();
}

void MainWindow::on_Set_Client_workingDirEdit_editingFinished()
{
    settings->beginGroup("Client_Env");
    settings->setValue("workingDir", ui->Set_Client_workingDirEdit->text());
    settings->endGroup();
}


void MainWindow::on_Set_Client_configPathEdit_editingFinished()
{
    settings->beginGroup("Client_Env");
    settings->setValue("extConfigFilePath", ui->Set_Client_configPathEdit->text());
    settings->endGroup();
}

void MainWindow::on_Set_Client_keepClientActiveBox_stateChanged(int arg1)
{
    settings->beginGroup("Client_keepClientActive");
    keepClientActive = (arg1 == Qt::Checked);
    settings->setValue("state", keepClientActive);
    settings->endGroup();
    emit setSerialListener(!keepClientActive);
}

void MainWindow::on_LF_LFConf_freqSlider_valueChanged(int value)
{
    onLFfreqConfChanged(value, true);
}

void MainWindow::onLFfreqConfChanged(int divisor, bool isCustomized)
{
    ui->LF_LFConf_freqDivisorBox->blockSignals(true);
    ui->LF_LFConf_freqSlider->blockSignals(true);

    if(isCustomized)
        ui->LF_LFConf_freqOtherButton->setChecked(true);
    else if(divisor == 95)
        ui->LF_LFConf_freq125kButton->setChecked(true);
    else if(divisor == 88)
        ui->LF_LFConf_freq134kButton->setChecked(true);
    ui->LF_LFConf_freqLabel->setText(tr("Actural Freq: ") + QString("%1kHz").arg(LF::divisor2Freq(divisor), 0, 'f', 3));
    ui->LF_LFConf_freqDivisorBox->setValue(divisor);
    ui->LF_LFConf_freqSlider->setValue(divisor);

    ui->LF_LFConf_freqDivisorBox->blockSignals(false);
    ui->LF_LFConf_freqSlider->blockSignals(false);
}

void MainWindow::on_LF_LFConf_freqDivisorBox_valueChanged(int arg1)
{
    onLFfreqConfChanged(arg1, true);
}

void MainWindow::on_LF_LFConf_freq125kButton_clicked()
{
    onLFfreqConfChanged(95, false);
}

void MainWindow::on_LF_LFConf_freq134kButton_clicked()
{
    onLFfreqConfChanged(88, false);
}

void MainWindow::on_LF_Op_searchButton_clicked()
{
    setState(false);
    lf->search();
    setState(true);
}

void MainWindow::on_LF_Op_readButton_clicked()
{
    setState(false);
    lf->read();
    setState(true);
}

void MainWindow::on_LF_Op_tuneButton_clicked()
{
    setState(false);
    lf->tune();
    setState(true);
}

void MainWindow::on_LF_Op_sniffButton_clicked()
{
    setState(false);
    lf->sniff();
    setState(true);
}

void MainWindow::dockInit()
{
    return;

    setDockNestingEnabled(true);
    QDockWidget* dock;
    QWidget* widget;
    int count = ui->funcTab->count();
    qDebug() << "dock count" << count;
    for(int i = 0; i < count; i++)
    {
        dock = new QDockWidget(ui->funcTab->tabText(0), this);
        qDebug() << "dock name" << ui->funcTab->tabText(0);
        dock->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);// movable is necessary, otherwise the dock cannot be dragged
        dock->setAllowedAreas(Qt::BottomDockWidgetArea);
        dock->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        widget = ui->funcTab->widget(0);
        dock->setWidget(widget);
        if(widget->objectName() == "rawTab")
            Util::setRawTab(dock, i);
        addDockWidget(Qt::BottomDockWidgetArea, dock);
        if(!dockList.isEmpty())
            tabifyDockWidget(dockList[0], dock);
        dockList.append(dock);
    }
    ui->funcTab->setVisible(false);
    dockList[0]->setVisible(true);
    dockList[0]->raise();
}

void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    Q_UNUSED(event);
}

void MainWindow::on_LF_LFConf_getButton_clicked()
{
    setState(false);
    lf->getLFConfig();
    setState(true);
}

void MainWindow::on_LF_LFConf_setButton_clicked()
{
    LF::LFConfig config;
    setState(false);
    config.divisor = ui->LF_LFConf_freqDivisorBox->value();
    config.bitsPerSample = ui->LF_LFConf_bitsPerSampleBox->value();
    config.decimation = ui->LF_LFConf_decimationBox->value();
    config.averaging = ui->LF_LFConf_averagingBox->isChecked();
    config.triggerThreshold = ui->LF_LFConf_thresholdBox->value();
    config.samplesToSkip = ui->LF_LFConf_skipsBox->value();
    lf->setLFConfig(config);
    Util::gotoRawTab();
    setState(true);
}

void MainWindow::on_LF_LFConf_resetButton_clicked()
{
    setState(false);
    lf->resetLFConfig();
    setState(true);
}

void MainWindow::on_Set_Client_configFileBox_currentIndexChanged(int index)
{
    ui->Set_Client_configPathEdit->setVisible(ui->Set_Client_configFileBox->itemData(index).toString() == "(ext)");
    settings->beginGroup("Client_Env");
    settings->setValue("configFile", ui->Set_Client_configFileBox->currentData());
    settings->endGroup();
}


void MainWindow::on_Set_UI_Opacity_Box_valueChanged(int arg1)
{
    Q_UNUSED(arg1);
    ui->Set_UI_Opacity_slider->blockSignals(true);
    ui->Set_UI_Opacity_Box->blockSignals(true);
    ui->Set_UI_Opacity_slider->setValue(100);
    ui->Set_UI_Opacity_Box->setValue(100);
    setWindowOpacity(1.0);
    ui->Set_UI_Opacity_Box->blockSignals(false);
    ui->Set_UI_Opacity_slider->blockSignals(false);
}


void MainWindow::on_Set_UI_Theme_setButton_clicked()
{
    settings->beginGroup("UI");
    settings->setValue("Theme_Name", ui->Set_UI_Theme_nameBox->currentData().toString());
    settings->endGroup();
}


void MainWindow::on_Set_UI_Font_setButton_clicked()
{
    QFont font = ui->Set_UI_Font_nameBox->currentFont();
    font.setPointSize(ui->Set_UI_Font_sizeBox->value());
    QApplication::setFont(font, "QWidget");

    settings->beginGroup("UI");
    settings->setValue("Font_Name", ui->Set_UI_Font_nameBox->currentFont().family());
    settings->setValue("Font_Size", ui->Set_UI_Font_sizeBox->value());
    settings->endGroup();
}


void MainWindow::on_Set_UI_DataFont_setButton_clicked()
{
    QFont font = ui->Set_UI_DataFont_nameBox->currentFont();
    font.setPointSize(ui->Set_UI_DataFont_sizeBox->value());
    ui->MF_dataWidget->setFont(font);
    ui->MF_keyWidget->setFont(font);

    settings->beginGroup("UI");
    settings->setValue("DataFont_Name", ui->Set_UI_DataFont_nameBox->currentFont().family());
    settings->setValue("DataFont_Size", ui->Set_UI_DataFont_sizeBox->value());
    settings->endGroup();
}


void MainWindow::on_Set_UI_CMDFont_setButton_clicked()
{
    QFont font = ui->Set_UI_CMDFont_nameBox->currentFont();
    font.setPointSize(ui->Set_UI_CMDFont_sizeBox->value());
    ui->Raw_outputEdit->setFont(font);

    settings->beginGroup("UI");
    settings->setValue("CMDFont_Name", ui->Set_UI_CMDFont_nameBox->currentFont().family());
    settings->setValue("CMDFont_Size", ui->Set_UI_CMDFont_sizeBox->value());
    settings->endGroup();
}

