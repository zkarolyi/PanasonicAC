#include <Arduino.h>
#include <ArduinoJson.h>
#include <unordered_map>

class AirConditioner
{
public:
    // Modes for the Air Conditioner
    enum class Mode
    {
        Auto = 0x04,
        Cool = 0x34,
        Heat = 0x44,
        Dry = 0x24,
        // FanOnly = 0x64,
        Off = 0x30
    };

    // Fan speeds for the Air Conditioner
    enum class FanSpeed
    {
        Auto = 0xA0,
        Speed1 = 0x30,
        Speed2 = 0x40,
        Speed3 = 0x50,
        Speed4 = 0x60,
        Speed5 = 0x70
    };

    // Swing modes for the Air Conditioner
    enum class SwingMode
    {
        Auto = 0xF0,
        Up = 0x10,
        UpCenter = 0x20,
        Center = 0x30,
        DownCenter = 0x40,
        Down = 0x50
    };

    // Presets for the Air Conditioner
    enum class Preset
    {
        Normal = 0x00,
        Powerful = 0x02,
        Quiet = 0x04
    };

    // AirConditioner class initialized or not
    enum class ACState
    {
        Initializing, // Before first query response is receive
        Ready,        // All done, ready to receive regular packets
    };

    // Constructor
    AirConditioner()
    {
        internalTemperature_ = 0.0;
        externalTemperature_ = 0.0;
        targetTemperature_ = 25.0;        // Default target temperature
        mode_ = Mode::Off;                // Default mode is Off
        fanSpeed_ = FanSpeed::Auto;       // Default fan speed is Auto
        swingMode_ = SwingMode::Auto;     // Default swing mode is Auto
        preset_ = Preset::Normal;         // Default preset is Normal
        acState_ = ACState::Initializing; // Default acState is Initializing
        data_.clear();
    }

    // Setters
    void loadFromPacket(String packet)
    {
        data_ = std::vector<uint8_t>(packet.begin() + 2, packet.begin() + 12);

        mode_ = (Mode)packet[2];
        fanSpeed_ = (FanSpeed)packet[5];
        swingMode_ = (SwingMode)packet[6];
        preset_ = (Preset)packet[7];

        targetTemperature_ = packet[3] * 0.5;
        if (packet[18] != 0x80)
            internalTemperature_ = packet[18];
        if (packet[19] != 0x80)
            externalTemperature_ = packet[19];

        if (acState_ != ACState::Ready)
        {
            acState_ = ACState::Ready;
        }
    }

    void setTargetTemperature(float temperature)
    {
        targetTemperature_ = temperature;
    }

    void setMode(const char *str)
    {
        mode_ = stringToMode(str, Mode::Off);
    }

    void setMode(Mode mode)
    {
        mode_ = mode;
    }

    void setFanSpeed(const char *str)
    {
        fanSpeed_ = stringToFanSpeed(str, FanSpeed::Auto);
    }

    void setFanSpeed(FanSpeed speed)
    {
        fanSpeed_ = speed;
    }

    void setSwingMode(const char *str)
    {
        swingMode_ = stringToSwingMode(str, SwingMode::Auto);
    }

    void setSwingMode(SwingMode mode)
    {
        swingMode_ = mode;
    }

    void setPreset(const char *str)
    {
        preset_ = stringToPreset(str, Preset::Normal);
    }

    void setPreset(Preset preset)
    {
        preset_ = preset;
    }

    // Getters
    float getInternalTemperature() const
    {
        return internalTemperature_;
    }

    float getExternalTemperature() const
    {
        return externalTemperature_;
    }

    float getTargetTemperature() const
    {
        return targetTemperature_;
    }

    Mode getMode() const
    {
        return mode_;
    }

    FanSpeed getFanSpeed() const
    {
        return fanSpeed_;
    }

    SwingMode getSwingMode() const
    {
        return swingMode_;
    }

    Preset getPreset() const
    {
        return preset_;
    }

    ACState getAcState() const
    {
        return acState_;
    }

    boolean ACReady() const
    {
        return acState_ == ACState::Ready;
    }

    std::vector<uint8_t> getData() const
    {
        std::vector<uint8_t> data(data_.size());
        for (size_t i = 0; i < data_.size(); ++i)
        {
            data[i] = data_[i];
        }

        data[0] = (int)mode_;
        data[3] = (int)fanSpeed_;
        data[4] = (int)swingMode_;
        data[5] = (int)preset_;

        data[1] = targetTemperature_ * 2;
        return data;
    }

    // Methods
    String toJsonStringStructured() const
    {
        StaticJsonDocument<300> jsonDocument;
        JsonObject root = jsonDocument.to<JsonObject>();

        root["acState"] = static_cast<int>(acState_);

        JsonObject temperatureObject = root.createNestedObject("temperature");
        temperatureObject["internal"] = internalTemperature_;
        temperatureObject["external"] = externalTemperature_;
        temperatureObject["target"] = targetTemperature_;

        JsonObject modeObject = root.createNestedObject("mode");
        modeObject["value"] = static_cast<int>(mode_);
        modeObject["text"] = modeToString(mode_);

        JsonObject fanSpeedObject = root.createNestedObject("fanSpeed");
        fanSpeedObject["value"] = static_cast<int>(fanSpeed_);
        fanSpeedObject["text"] = fanSpeedToString(fanSpeed_);

        JsonObject swingModeObject = root.createNestedObject("swingMode");
        swingModeObject["value"] = static_cast<int>(swingMode_);
        swingModeObject["text"] = swingModeToString(swingMode_);

        JsonObject presetObject = root.createNestedObject("preset");
        presetObject["value"] = static_cast<int>(preset_);
        presetObject["text"] = presetToString(preset_);

        String jsonString;
        serializeJson(root, jsonString);
        return jsonString;
    }

private:
    // Data members
    float internalTemperature_;
    float externalTemperature_;
    float targetTemperature_;
    Mode mode_;
    FanSpeed fanSpeed_;
    SwingMode swingMode_;
    Preset preset_;
    ACState acState_;
    std::vector<uint8_t> data_;

    // Helper functions to convert enum values to strings and back
    template <typename Enum>
    const char *enumToString(Enum enumValue, const std::unordered_map<std::string, Enum> &enumMap) const
    {
        for (const auto &entry : enumMap)
        {
            if (entry.second == enumValue)
            {
                return entry.first.c_str();
            }
        }
        return "Unknown";
    }

    template <typename Enum>
    Enum stringToEnum(const char *str, const std::unordered_map<std::string, Enum> &enumMap, Enum defaultValue) const
    {
        auto it = enumMap.find(str);
        if (it != enumMap.end())
        {
            return it->second;
        }
        return defaultValue;
    }

    std::unordered_map<std::string, Mode> modeMap = {
        {"Auto", Mode::Auto},
        {"Cool", Mode::Cool},
        {"Heat", Mode::Heat},
        {"Dry", Mode::Dry},
        // {"FanOnly", Mode::FanOnly},
        {"Off", Mode::Off}};

    std::unordered_map<std::string, FanSpeed> fanSpeedMap = {
        {"Auto", FanSpeed::Auto},
        {"1", FanSpeed::Speed1},
        {"2", FanSpeed::Speed2},
        {"3", FanSpeed::Speed3},
        {"4", FanSpeed::Speed4},
        {"5", FanSpeed::Speed5}};

    std::unordered_map<std::string, SwingMode> swingModeMap = {
        {"Auto", SwingMode::Auto},
        {"Up", SwingMode::Up},
        {"UpCenter", SwingMode::UpCenter},
        {"Center", SwingMode::Center},
        {"DownCenter", SwingMode::DownCenter},
        {"Down", SwingMode::Down}};

    std::unordered_map<std::string, Preset> presetMap = {
        {"Normal", Preset::Normal},
        {"Powerful", Preset::Powerful},
        {"Quiet", Preset::Quiet}};

    const char *modeToString(Mode mode) const
    {
        return enumToString(mode, modeMap);
    }

    Mode stringToMode(const char *str, Mode defaultValue) const
    {
        return stringToEnum(str, modeMap, defaultValue);
    }

    const char *fanSpeedToString(FanSpeed enumValue) const
    {
        return enumToString(enumValue, fanSpeedMap);
    }

    FanSpeed stringToFanSpeed(const char *str, FanSpeed defaultValue) const
    {
        return stringToEnum(str, fanSpeedMap, defaultValue);
    }

    const char *swingModeToString(SwingMode mode) const
    {
        return enumToString(mode, swingModeMap);
    }

    SwingMode stringToSwingMode(const char *str, SwingMode defaultValue) const
    {
        return stringToEnum(str, swingModeMap, defaultValue);
    }

    const char *presetToString(Preset preset) const
    {
        return enumToString(preset, presetMap);
    }

    Preset stringToPreset(const char *str, Preset defaultValue) const
    {
        return stringToEnum(str, presetMap, defaultValue);
    }
};
