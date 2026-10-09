#include "AulaHEPlugin.h"
#include "AulaKeymap.h"

#include <QColorDialog>
#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <QScrollArea>
#include <QVBoxLayout>
#include <chrono>

OpenRGBPluginInfo AulaHEPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info{};
    info.Name = "OpenRGB-WIN68";
    info.Description = "A OpenRGB Plugin That's Connect AULA WIN68 HE to OpenRGB Without OEM Software.";
    info.Version = "1.0.0";
    info.Commit = "Bardia Dehbozorgi";
    info.URL = "https://github.com/BARDIA156/OpenRGB-WIN68";
    // Keep the plugin control page out of Devices.  The physical keyboard is
    // represented by the virtual controller only, so OpenRGB no longer shows
    // a confusing second "AULA HE" tile beside the device.
    info.Icon = QImage(":/icons/openrgb-win68.png");
    info.TabIcon = QImage(":/icons/openrgb-win68.png");
    info.Location = OPENRGB_PLUGIN_LOCATION_SETTINGS;
    info.Label = "OpenRGB-WIN68";
    info.ProtocolVersion = OPENRGB_PLUGIN_API_VERSION;
    return info;
}

unsigned int AulaHEPlugin::GetPluginAPIVersion()
{
    return OPENRGB_PLUGIN_API_VERSION;
}

void AulaHEPlugin::Load(OpenRGBPluginAPIInterface* plugin_api_ptr)
{
    api_ = plugin_api_ptr;
    device_ = std::make_unique<AulaHEDevice>();
    if(device_->Connect())
    {
        RGBController_Setup setup = device_->CreateControllerSetup();
        controller_ = api_->CreateVirtualRGBController(&setup);
        device_->AttachController(controller_);
        // Start after the plugin scan returns to Qt's event loop, then register
        // off the GUI thread. Effects handles device-list updates through a
        // blocking GUI callback; this also keeps startup registration ordered.
        QTimer::singleShot(0, this, [this]() {
            if(!api_ || !controller_) return;
            registration_ = std::async(std::launch::async, [this]() {
                api_->RegisterVirtualRGBController(controller_);
            });
        });
        if(status_) status_->setText("AULA WIN 68 HE connected");
    }
    else if(status_)
    {
        status_->setText("AULA WIN 68 HE not found — connect it, then reload the plugin.");
    }
}

QWidget* AulaHEPlugin::GetWidget()
{
    if(!widget_)
    {
        widget_ = new QWidget();
        auto* layout = new QVBoxLayout(widget_);
        status_ = new QLabel(device_ && device_->IsConnected()
                             ? "AULA WIN 68 HE connected"
                             : "AULA WIN 68 HE not found; connect it and reload the plugin.", widget_);
        layout->addWidget(status_);
        layout->addWidget(new QLabel("Click a labeled key to pick its color. Select Direct in Devices for per-key lighting.", widget_));

        auto* scroll = new QScrollArea(widget_);
        scroll->setWidgetResizable(false);
        auto* board = new QWidget(scroll);
        board->setFixedSize(940, 330);
        key_buttons_.reserve(68);
        for(unsigned int i = 0; i < 68; ++i)
        {
            const AulaKey& key = AULA_KEYS[i];
            auto* button = new QPushButton(QString::fromUtf8(key.legend), board);
            button->setGeometry(8 + key.x * 3 / 2, 18 + key.y * 3 / 2,
                                key.width * 3 / 2, key.height * 3 / 2);
            QFont font = button->font();
            font.setPointSize(7);
            button->setFont(font);
            button->setToolTip(QString("%1 | key %2 | firmware index %3")
                                   .arg(QString::fromUtf8(key.name)).arg(i).arg(key.firmware_index));
            connect(button, &QPushButton::clicked, widget_, [this, i]() {
                if(!controller_) return;
                const RGBColor current = controller_->GetZoneColor(0, i);
                const QColor initial(RGBGetRValue(current), RGBGetGValue(current), RGBGetBValue(current));
                const QColor selected = QColorDialog::getColor(initial, widget_, QString::fromUtf8(AULA_KEYS[i].name));
                if(!selected.isValid()) return;
                controller_->SetActiveMode(0); // Direct
                controller_->SetColor(i, ToRGBColor(selected.red(), selected.green(), selected.blue()));
                controller_->UpdateSingleLED(i);
                RefreshKeyboardPreview();
            });
            key_buttons_.push_back(button);
        }
        scroll->setWidget(board);
        layout->addWidget(scroll);
        auto* sdk_hint = new QLabel("For Python: run the SDK server in the same OpenRGB instance and enable All Controllers. "
                                   "Effects should list this keyboard under Direct.", widget_);
        sdk_hint->setWordWrap(true);
        layout->addWidget(sdk_hint);
        auto* input_warning = new QLabel("Hardware warning: very frequent Custom / Per-Key color updates can make some key presses stick or go missing. "
                                        "Stop the effect if input behaves incorrectly. The plugin does not cap your effect FPS.", widget_);
        input_warning->setWordWrap(true);
        input_warning->setStyleSheet("color: #e6ad55;");
        layout->addWidget(input_warning);
        preview_timer_ = new QTimer(widget_);
        connect(preview_timer_, &QTimer::timeout, widget_, [this]() { RefreshKeyboardPreview(); });
        preview_timer_->start(250);
        RefreshKeyboardPreview();
        layout->addStretch();
    }
    return widget_;
}

void AulaHEPlugin::RefreshKeyboardPreview()
{
    if(!controller_ || !device_) return;
    if(status_)
        status_->setText(QString("WIN68 connected | HID reports: %1 | write errors: %2")
                             .arg(device_->ReportsSent()).arg(device_->WriteErrors()));
    for(unsigned int i = 0; i < key_buttons_.size(); ++i)
    {
        const RGBColor color = controller_->GetZoneColor(0, i);
        const int r = RGBGetRValue(color), g = RGBGetGValue(color), b = RGBGetBValue(color);
        const bool light = (r * 299 + g * 587 + b * 114) > 140000;
        key_buttons_[i]->setStyleSheet(QString("background-color: rgb(%1,%2,%3); color: %4;")
                                          .arg(r).arg(g).arg(b).arg(light ? "black" : "white"));
    }
}

QMenu* AulaHEPlugin::GetTrayMenu() { return nullptr; }

void AulaHEPlugin::Unload()
{
    if(preview_timer_) preview_timer_->stop();
    if(registration_.valid())
    {
        // Let the UI service Effects' blocking device-list callback while we
        // wait; the API and controller must outlive the registration thread.
        while(registration_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        registration_.get();
    }
    if(api_ && controller_)
    {
        api_->UnregisterVirtualRGBController(controller_);
        api_->DeleteVirtualRGBController(controller_);
        controller_ = nullptr;
        if(device_) device_->AttachController(nullptr);
    }
    if(device_)
    {
        device_->Disconnect();
        device_.reset();
    }
    api_ = nullptr;
}

void AulaHEPlugin::OnProfileAboutToLoad() {}
void AulaHEPlugin::OnProfileLoad(nlohmann::json) {}
nlohmann::json AulaHEPlugin::OnProfileSave() { return nlohmann::json::object(); }
unsigned char* AulaHEPlugin::OnSDKCommand(unsigned int, unsigned char*, unsigned int*) { return nullptr; }
void AulaHEPlugin::ProfileManagerUpdated(unsigned int) {}
void AulaHEPlugin::ResourceManagerUpdated(unsigned int) {}
void AulaHEPlugin::SettingsManagerUpdated(unsigned int) {}
