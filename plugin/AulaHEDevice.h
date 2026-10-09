#pragma once

#include <array>
#include <climits>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include <hidapi.h>

#include "RGBControllerInterface.h"

class AulaHEDevice
{
public:
    AulaHEDevice();
    ~AulaHEDevice();

    bool Connect();
    void Disconnect();
    bool IsConnected() const;
    unsigned long long ReportsSent() const { return reports_sent_.load(); }
    unsigned long long WriteErrors() const { return write_errors_.load(); }

    RGBController_Setup CreateControllerSetup();
    void AttachController(RGBControllerInterface* controller);

    static void UpdateModeCallback(void* context);
    static void UpdateLEDsCallback(void* context);
    static void UpdateZoneLEDsCallback(void* context, int zone);
    static void UpdateSingleLEDCallback(void* context, int led);

private:
    static constexpr unsigned short AULA_VID = 0x2E3C;
    static constexpr unsigned short AULA_PID = 0xC365;
    static constexpr unsigned short AULA_USAGE_PAGE = 0xFF1B;
    static constexpr unsigned char REPORT_ID = 0x01;

    bool WriteLighting(unsigned char firmware_mode, unsigned char brightness,
                       unsigned char speed, RGBColor foreground, RGBColor background,
                       unsigned char direction, bool full_color);
    bool WriteCustomColors(const RGBColor* colors, unsigned int count);
    void ApplySelectedMode();
    static unsigned char ToAulaDirection(unsigned int openrgb_direction);

    hid_device* device_ = nullptr;
    RGBControllerInterface* controller_ = nullptr;
    std::string location_;
    std::array<unsigned char, 64> last_report_{};
    bool has_last_report_ = false;
    std::atomic<unsigned long long> reports_sent_{0};
    std::atomic<unsigned long long> write_errors_{0};
    std::array<unsigned char, 396> last_palette_{};
    std::array<bool, 8> page_valid_{};
    RGBColor remembered_foreground_ = ToRGBColor(255, 255, 255);
    RGBColor remembered_background_ = ToRGBColor(0, 0, 0);
    unsigned int remembered_mode_ = UINT_MAX;
    bool remembered_colors_valid_ = false;
    mutable std::mutex mutex_;
};
