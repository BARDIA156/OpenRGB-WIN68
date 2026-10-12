#pragma once

#include <memory>
#include <future>

#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTimer>
#include <QWidget>
#include <vector>

#include "OpenRGBPluginInterface.h"

#include "AulaHEDevice.h"

class AulaHEPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "AulaHEPlugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    OpenRGBPluginInfo GetPluginInfo() override;
    unsigned int GetPluginAPIVersion() override;
    void Load(OpenRGBPluginAPIInterface* plugin_api_ptr) override;
    QWidget* GetWidget() override;
    QMenu* GetTrayMenu() override;
    void Unload() override;
    void OnProfileAboutToLoad() override;
    void OnProfileLoad(nlohmann::json profile_data) override;
    nlohmann::json OnProfileSave() override;
    unsigned char* OnSDKCommand(unsigned int pkt_id, unsigned char* pkt_data, unsigned int* pkt_size) override;
    void ProfileManagerUpdated(unsigned int update_reason) override;
    void ResourceManagerUpdated(unsigned int update_reason) override;
    void SettingsManagerUpdated(unsigned int update_reason) override;

private:
    void RefreshKeyboardPreview();
    OpenRGBPluginAPIInterface* api_ = nullptr;
    std::future<void> registration_;
    std::unique_ptr<AulaHEDevice> device_;
    RGBControllerInterface* controller_ = nullptr;
    QWidget* widget_ = nullptr;
    QLabel* status_ = nullptr;
    QTimer* preview_timer_ = nullptr;
    std::vector<QPushButton*> key_buttons_;
};
