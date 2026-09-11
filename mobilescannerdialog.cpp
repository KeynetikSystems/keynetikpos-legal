#include "mobilescannerdialog.h"
#include "posapiserver.h"
#include "appstyle.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

MobileScannerDialog::MobileScannerDialog(PosApiServer *server, QWidget *parent)
    : QDialog(parent)
    , m_server(server)
{
    setupUI();

    connect(m_server, &PosApiServer::devicePaired, this, &MobileScannerDialog::refreshStatus);
    connect(m_server, &PosApiServer::pinRegenerated, this, &MobileScannerDialog::refreshStatus);

    refreshStatus();
}

void MobileScannerDialog::setupUI()
{
    setWindowTitle("Mobile Scanner");
    setMinimumSize(420, 320);

    auto *mainLayout = new QVBoxLayout(this);

    auto *titleLabel = new QLabel("Mobile Scanner", this);
    titleLabel->setProperty("role", "chip");
    titleLabel->setProperty("kind", "info");
    titleLabel->setProperty("textScale", "xl");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    auto *helpLabel = new QLabel(
        "On the phone app, enter this till's address and PIN to pair it as a "
        "barcode scanner. Both devices must be on the same WiFi network.", this);
    helpLabel->setWordWrap(true);
    mainLayout->addWidget(helpLabel);

    auto *connectionBox = new QGroupBox("Connection", this);
    auto *connectionLayout = new QVBoxLayout(connectionBox);

    m_addressLabel = new QLabel(connectionBox);
    m_addressLabel->setProperty("textScale", "md");
    m_addressLabel->setProperty("bold", true);
    m_addressLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    connectionLayout->addWidget(m_addressLabel);

    m_pinLabel = new QLabel(connectionBox);
    m_pinLabel->setProperty("role", "statValueLg");
    m_pinLabel->setProperty("kind", "primary");
    m_pinLabel->setAlignment(Qt::AlignCenter);
    m_pinLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    connectionLayout->addWidget(m_pinLabel);

    m_statusChip = new QLabel(connectionBox);
    m_statusChip->setProperty("role", "chip");
    m_statusChip->setAlignment(Qt::AlignCenter);
    connectionLayout->addWidget(m_statusChip);

    mainLayout->addWidget(connectionBox);

    auto *buttonLayout = new QHBoxLayout();
    m_regenerateBtn = new QPushButton("Regenerate PIN", this);
    m_regenerateBtn->setProperty("kind", "warning");
    connect(m_regenerateBtn, &QPushButton::clicked,
            this, &MobileScannerDialog::onRegenerateClicked);
    buttonLayout->addWidget(m_regenerateBtn);

    buttonLayout->addStretch();

    m_closeBtn = new QPushButton("Close", this);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::close);
    buttonLayout->addWidget(m_closeBtn);

    mainLayout->addLayout(buttonLayout);
}

void MobileScannerDialog::onRegenerateClicked()
{
    m_server->regeneratePin();
}

void MobileScannerDialog::refreshStatus()
{
    if (!m_server->isListening()) {
        m_addressLabel->setText("Server is starting…");
    } else {
        const QStringList addrs = PosApiServer::lanAddresses();
        const QString address = addrs.isEmpty() ? QStringLiteral("no LAN address found")
                                                 : addrs.first();
        m_addressLabel->setText(
            QString("Address: %1:%2").arg(address).arg(m_server->port()));
    }

    m_pinLabel->setText("PIN: " + m_server->currentPin());

    const bool paired = m_server->isPaired();
    m_statusChip->setText(paired ? "Paired" : "Not paired yet");
    setStyleProperty(m_statusChip, "kind", paired ? "success" : "danger");
}
