#include "AulaHEDevice.h"
#include "AulaKeymap.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <cwchar>

namespace
{
struct AulaModeDefinition
{
    const char* name;
    unsigned char firmware_code;
    bool has_speed;
    unsigned int direction_flags;
    bool has_background;
    bool full_color;
};

// Firmware mode bytes/control rules mirror Aether's WIN68HE registry.  The
// `full_color` byte is the firmware rainbow/full-spectrum switch. It is
// enabled only when the user selects OpenRGB's Random color option.
// Names/bytes/directions match Aether-HE's verified WIN68/WIN60 registry.
// OpenRGB's direction widget has six useful values here: LR, UD, horizontal
// and vertical are mapped to right/left/up/down/spread/gather respectively.
constexpr AulaModeDefinition AULA_MODES[] = {
    { "Direct",                   10,  false, 0,                                                        false, false },
    { "Static",                     0,   false, 0,                                                        false, true  },
    { "Breath",                     1,   true,  0,                                                        true,  true  },
    { "Wave",                       2,   true,  MODE_FLAG_HAS_DIRECTION_LR | MODE_FLAG_HAS_DIRECTION_UD | MODE_FLAG_HAS_DIRECTION_HV, true, true  },
    { "Neon",                       3,   true,  0,                                                        true,  true  },
    { "Radar",                      4,   true,  MODE_FLAG_HAS_DIRECTION_LR,                                true,  true  },
    { "Reactive",                   6,   true,  0,                                                        true,  true  },
    { "Aurora",                     7,   true,  MODE_FLAG_HAS_DIRECTION_HV,                               true,  true  },
    { "Ripple",                     8,   true,  0,                                                        true,  true  },
    { "Twinkle",                    9,   true,  0,                                                        true,  true  },
    { "Custom (per-key palette)",  10,  false, 0,                                                        false, false },
    { "Cross",                     11,   true,  0,                                                        true,  true  },
    { "Speed Respond",             12,   false, MODE_FLAG_HAS_DIRECTION_UD,                              true,  true  },
    { "Auto Ripple",               14,   true,  0,                                                        true,  true  },
    { "Striation",                 15,   true,  MODE_FLAG_HAS_DIRECTION_LR,                              true,  true  },
    { "Fireworks",                 16,   true,  0,                                                        true,  true  },
    { "Frenzy (Fireworks alias)",  16,   true,  0,                                                        true,  true  },
};

constexpr unsigned int AULA_LED_COUNT = 68;
static_assert(std::size(AULA_KEYS) == AULA_LED_COUNT, "WIN68 layout must have 68 keys");

unsigned char ClampAulaValue(unsigned int value)
{
    return static_cast<unsigned char>(std::min(4u, value));
}
}

AulaHEDevice::AulaHEDevice() = default;

AulaHEDevice::~AulaHEDevice()
{
    Disconnect();
}

bool AulaHEDevice::Connect()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(device_)
    {
        return true;
    }

    struct hid_device_info* devices = hid_enumerate(AULA_VID, AULA_PID);
    struct hid_device_info* matched = nullptr;
    struct hid_device_info* fallback = nullptr;
    for(struct hid_device_info* info = devices; info; info = info->next)
    {
        if(info->usage_page == AULA_USAGE_PAGE)
        {
            // Prefer the real WIN 68 product string when several AULA
            // boards share VID:PID 2E3C:C365.
            if(info->product_string &&
               (std::wcsstr(info->product_string, L"WIN 68") != nullptr ||
                std::wcsstr(info->product_string, L"SI2828HEARGB") != nullptr ||
                std::wcsstr(info->product_string, L"SI2828KZHEARGB") != nullptr))
            {
                matched = info;
                break;
            }
            if(!fallback)
            {
                fallback = info;
            }
        }
    }
    // VID:PID is shared with WIN60 and KP-TE153. An unknown product string
    // cannot safely be treated as a WIN68: it would expose the wrong keymap.
    if(!matched && fallback && fallback->product_string &&
       (std::wcsstr(fallback->product_string, L"WIN 68") != nullptr ||
        std::wcsstr(fallback->product_string, L"SI2828") != nullptr)) matched = fallback;

    if(!matched)
    {
        hid_free_enumeration(devices);
        return false;
    }

    device_ = hid_open_path(matched->path);
    if(device_)
    {
        location_ = matched->path ? matched->path : "AULA HID interface";
        hid_set_nonblocking(device_, 0);

        // Aether sends this lightweight cmd-1 heartbeat after opening the
        // vendor interface.  It does not change lighting/configuration, but
        // makes the plugin follow the proven AULA connection sequence.
        std::array<unsigned char, 64> heartbeat{};
        heartbeat[0] = REPORT_ID;
        heartbeat[1] = 0x01;
        if(hid_write(device_, heartbeat.data(), heartbeat.size()) == static_cast<int>(heartbeat.size()))
            ++reports_sent_;
        else
            ++write_errors_;
    }
    hid_free_enumeration(devices);
    return device_ != nullptr;
}

void AulaHEDevice::Disconnect()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(device_)
    {
        hid_close(device_);
        device_ = nullptr;
    }
    has_last_report_ = false;
    page_valid_.fill(false);
}

bool AulaHEDevice::IsConnected() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return device_ != nullptr;
}

RGBController_Setup AulaHEDevice::CreateControllerSetup()
{
    RGBController_Setup setup{};
    setup.name = "AULA WIN 68 HE";
    setup.vendor = "AULA";
    setup.description = "AULA WIN 68 HE RGB";
    setup.version = "OpenRGB-AulaHE 0.7.0";
    setup.location = location_;
    setup.type = DEVICE_TYPE_KEYBOARD;
    setup.flags = CONTROLLER_FLAG_VIRTUAL;
    setup.active_mode = 0;
    setup.object_ptr = this;
    setup.DeviceUpdateMode = UpdateModeCallback;
    setup.DeviceUpdateLEDs = UpdateLEDsCallback;
    setup.DeviceUpdateZoneLEDs = UpdateZoneLEDsCallback;
    setup.DeviceUpdateSingleLED = UpdateSingleLEDCallback;

    for(unsigned int i = 0; i < AULA_LED_COUNT; ++i)
    {
        led key;
        key.name = AULA_KEYS[i].openrgb_name;
        key.value = AULA_KEYS[i].firmware_index;
        setup.leds.push_back(key);
    }

    for(const AulaModeDefinition& definition : AULA_MODES)
    {
        mode openrgb_mode;
        openrgb_mode.name = definition.name;
        openrgb_mode.value = definition.firmware_code;
        openrgb_mode.flags = MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_REQUIRES_ENTIRE_DEVICE;
        if(definition.firmware_code != 10) openrgb_mode.flags |= MODE_FLAG_AUTOMATIC_SAVE;
        if(definition.firmware_code == 10)
        {
            openrgb_mode.flags |= MODE_FLAG_HAS_PER_LED_COLOR;
        }
        else
        {
            openrgb_mode.flags |= MODE_FLAG_HAS_MODE_SPECIFIC_COLOR;
        }
        if(definition.has_speed)
        {
            openrgb_mode.flags |= MODE_FLAG_HAS_SPEED;
        }
        if(definition.full_color)
        {
            openrgb_mode.flags |= MODE_FLAG_HAS_RANDOM_COLOR;
        }
        openrgb_mode.flags |= definition.direction_flags;
        openrgb_mode.brightness_min = 0;
        openrgb_mode.brightness_max = 4;
        openrgb_mode.brightness = 4;
        openrgb_mode.speed_min = 0;
        openrgb_mode.speed_max = 4;
        openrgb_mode.speed = 4;
        openrgb_mode.colors_min = definition.firmware_code == 10 ? 0 : (definition.has_background ? 2 : 1);
        openrgb_mode.colors_max = definition.firmware_code == 10 ? 0 : (definition.has_background ? 2 : 1);
        openrgb_mode.color_mode = definition.firmware_code == 10 ? MODE_COLORS_PER_LED : MODE_COLORS_MODE_SPECIFIC;
        if(definition.firmware_code != 10)
        {
            openrgb_mode.colors.push_back(ToRGBColor(255, 255, 255));
            if(definition.has_background)
            {
                openrgb_mode.colors.push_back(ToRGBColor(0, 0, 0));
            }
        }
        setup.modes.push_back(openrgb_mode);
    }

    // One real 68-LED zone prevents Apply-All from touching a fake one-LED
    // controller and gives SDK clients a stable per-key LED array.
    zone whole_keyboard;
    whole_keyboard.name = "AULA WIN68 HE Keys";
    whole_keyboard.display_name = "Keyboard Keys";
    whole_keyboard.type = ZONE_TYPE_MATRIX;
    whole_keyboard.leds_count = AULA_LED_COUNT;
    whole_keyboard.leds_min = AULA_LED_COUNT;
    whole_keyboard.leds_max = AULA_LED_COUNT;
    whole_keyboard.matrix_map.height = 5;
    whole_keyboard.matrix_map.width = 16;
    whole_keyboard.matrix_map.map.assign(5 * 16, 0xFFFFFFFFu);
    // One cell per 38-pixel vendor key pitch, aligned by left edge. OpenRGB
    // 1.0 expands certain standardized wide key names into adjacent empty
    // cells. Arbitrary pixel-exact widths are not supported in Devices view.
    // Map entries are OpenRGB LED ordinals, not the firmware indices.
    for(unsigned int i = 0; i < AULA_LED_COUNT; ++i)
        whole_keyboard.matrix_map.map[AULA_KEYS[i].row * 16 + AULA_KEYS[i].column] = i;
    whole_keyboard.active_mode = -1;
    setup.zones.push_back(whole_keyboard);
    return setup;
}

void AulaHEDevice::AttachController(RGBControllerInterface* controller)
{
    controller_ = controller;
}

void AulaHEDevice::UpdateModeCallback(void* context)
{
    static_cast<AulaHEDevice*>(context)->ApplySelectedMode();
}

void AulaHEDevice::UpdateLEDsCallback(void* context)
{
    static_cast<AulaHEDevice*>(context)->ApplySelectedMode();
}

void AulaHEDevice::UpdateZoneLEDsCallback(void* context, int /*zone*/)
{
    static_cast<AulaHEDevice*>(context)->ApplySelectedMode();
}

void AulaHEDevice::UpdateSingleLEDCallback(void* context, int /*led*/)
{
    // Python SDK set_color/update_led uses this path too. Page caching below
    // sends only the page containing the changed key.
    static_cast<AulaHEDevice*>(context)->ApplySelectedMode();
}

unsigned char AulaHEDevice::ToAulaDirection(unsigned int openrgb_direction)
{
    // OpenRGB: left=0/right=1/up=2/down=3. AULA: right=0/left=1/up=2/down=3.
    if(openrgb_direction == MODE_DIRECTION_LEFT)  return 1;
    if(openrgb_direction == MODE_DIRECTION_RIGHT) return 0;
    if(openrgb_direction == MODE_DIRECTION_UP)    return 2;
    if(openrgb_direction == MODE_DIRECTION_DOWN)  return 3;
    if(openrgb_direction == MODE_DIRECTION_HORIZONTAL) return 4; // spread
    if(openrgb_direction == MODE_DIRECTION_VERTICAL)   return 5; // gather
    return 0;
}

void AulaHEDevice::ApplySelectedMode()
{
    if(!controller_ || !IsConnected())
    {
        return;
    }
    const int active_mode = controller_->GetActiveMode();
    if(active_mode < 0 || static_cast<size_t>(active_mode) >= std::size(AULA_MODES))
    {
        return;
    }

    const unsigned int mode_index = static_cast<unsigned int>(active_mode);
    if(AULA_MODES[mode_index].firmware_code == 10)
    {
        if(WriteLighting(10, ClampAulaValue(controller_->GetModeBrightness(mode_index)), 4,
                         ToRGBColor(255, 255, 255), ToRGBColor(0, 0, 0), 0, false))
        {
            std::array<RGBColor, AULA_LED_COUNT> colors{};
            for(unsigned int i = 0; i < AULA_LED_COUNT; ++i)
                colors[i] = controller_->GetZoneColor(0, i);
            WriteCustomColors(colors.data(), AULA_LED_COUNT);
        }
        return;
    }
    page_valid_.fill(false);

    RGBColor foreground = ToRGBColor(255, 255, 255);
    RGBColor background = ToRGBColor(0, 0, 0);
    if(controller_->GetModeColorsCount(mode_index) > 0)
    {
        foreground = controller_->GetModeColor(mode_index, 0);
    }
    if(AULA_MODES[mode_index].has_background && controller_->GetModeColorsCount(mode_index) > 1)
    {
        background = controller_->GetModeColor(mode_index, 1);
    }

    // Keep one user-selected palette when switching between firmware effects.
    // OpenRGB creates a fresh default palette for each mode, so replace only
    // that untouched default after the first mode has been used.
    const RGBColor default_foreground = ToRGBColor(255, 255, 255);
    const RGBColor default_background = ToRGBColor(0, 0, 0);
    if(remembered_mode_ == mode_index)
    {
        remembered_foreground_ = foreground;
        remembered_background_ = background;
        remembered_colors_valid_ = true;
    }
    else if(remembered_colors_valid_ && foreground == default_foreground &&
            (!AULA_MODES[mode_index].has_background || background == default_background))
    {
        foreground = remembered_foreground_;
        if(AULA_MODES[mode_index].has_background)
        {
            background = remembered_background_;
        }
    }
    remembered_mode_ = mode_index;

    const bool full_color = AULA_MODES[mode_index].full_color &&
        controller_->GetModeColorMode(mode_index) == MODE_COLORS_RANDOM;
    WriteLighting(AULA_MODES[mode_index].firmware_code,
                  ClampAulaValue(controller_->GetModeBrightness(mode_index)),
                  ClampAulaValue(controller_->GetModeSpeed(mode_index)),
                  foreground, background,
                  ToAulaDirection(controller_->GetModeDirection(mode_index)),
                  full_color);
}

bool AulaHEDevice::WriteCustomColors(const RGBColor* colors, unsigned int count)
{
    if(!colors || count == 0)
    {
        return false;
    }
    std::array<unsigned char, 396> table{};
    const unsigned int limit = std::min<unsigned int>(count, table.size() / 3);
    for(unsigned int i = 0; i < limit; ++i)
    {
        const unsigned int index = AULA_KEYS[i].firmware_index;
        table[index * 3 + 0] = RGBGetRValue(colors[i]);
        table[index * 3 + 1] = RGBGetGValue(colors[i]);
        table[index * 3 + 2] = RGBGetBValue(colors[i]);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if(!device_) return false;
    for(unsigned int page = 0, offset = 0; offset < table.size(); ++page, offset += 54)
    {
        const unsigned int length = std::min<unsigned int>(54, static_cast<unsigned int>(table.size() - offset));
        if(page_valid_[page] && std::equal(table.data() + offset, table.data() + offset + length,
                                          last_palette_.data() + offset)) continue;
        std::array<unsigned char, 64> report{};
        report[0] = REPORT_ID;
        report[1] = 0x09;
        report[2] = 0; // persistent Custom1 slot
        report[3] = static_cast<unsigned char>((page >> 8) & 0xFF);
        report[4] = static_cast<unsigned char>(page & 0xFF);
        report[5] = static_cast<unsigned char>(length);
        std::copy_n(table.data() + offset, length, report.data() + 6);
        if(hid_write(device_, report.data(), report.size()) != static_cast<int>(report.size()))
        {
            ++write_errors_;
            page_valid_[page] = false;
            return false;
        }
        ++reports_sent_;
        std::copy_n(table.data() + offset, length, last_palette_.data() + offset);
        page_valid_[page] = true;
    }
    return true;
}

bool AulaHEDevice::WriteLighting(unsigned char firmware_mode, unsigned char brightness,
                                 unsigned char speed, RGBColor foreground, RGBColor background,
                                 unsigned char direction, bool full_color)
{
    std::array<unsigned char, 64> report{};
    report[0] = REPORT_ID;
    report[1] = 0x07; // firmware lighting command
    report[5] = 0x0E; // payload size
    report[6] = firmware_mode;
    report[7] = brightness;
    report[8] = speed;
    report[9] = RGBGetRValue(foreground);
    report[10] = RGBGetGValue(foreground);
    report[11] = RGBGetBValue(foreground);
    report[12] = RGBGetRValue(background);
    report[13] = RGBGetGValue(background);
    report[14] = RGBGetBValue(background);
    report[15] = direction;
    report[16] = full_color ? 1 : 0;
    report[17] = 0; // power on

    std::lock_guard<std::mutex> lock(mutex_);
    if(!device_)
    {
        return false;
    }
    if(has_last_report_ && report == last_report_)
    {
        return true;
    }
    const int written = hid_write(device_, report.data(), report.size());
    if(written == static_cast<int>(report.size()))
    {
        ++reports_sent_;
        last_report_ = report;
        has_last_report_ = true;
        return true;
    }
    has_last_report_ = false;
    ++write_errors_;
    return false;
}
