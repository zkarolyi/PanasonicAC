#include <Arduino.h>
#include <ArduinoJson.h>

class AirConditioner
{
public:
    // Modes for the Air Conditioner
    enum class Mode
    {
        HeatCool = 0x04,
        Cool = 0x34,
        Heat = 0x44,
        Dry = 0x24,
        FanOnly = 0x64,
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

        acState_ = ACState::Ready;
    }

    void setTargetTemperature(float temperature)
    {
        targetTemperature_ = temperature;
    }

    void setMode(Mode mode)
    {
        mode_ = mode;
    }

    void setFanSpeed(FanSpeed speed)
    {
        fanSpeed_ = speed;
    }

    void setSwingMode(SwingMode mode)
    {
        swingMode_ = mode;
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

    std::vector<uint8_t> getData() const
    {
        return data_;
    }

    // Methods
    String toJsonString() const
    {
        StaticJsonDocument<200> jsonDocument;
        JsonObject jsonObject = jsonDocument.to<JsonObject>();

        jsonObject["internalTemperature"] = internalTemperature_;
        jsonObject["externalTemperature"] = externalTemperature_;
        jsonObject["targetTemperature"] = targetTemperature_;
        jsonObject["mode"] = static_cast<int>(mode_);
        jsonObject["modeText"] = modeToString(mode_);
        jsonObject["fanSpeed"] = static_cast<int>(fanSpeed_);
        jsonObject["fanSpeedText"] = fanSpeedToString(fanSpeed_);
        jsonObject["swingMode"] = static_cast<int>(swingMode_);
        jsonObject["swingModeText"] = swingModeToString(swingMode_);
        jsonObject["preset"] = static_cast<int>(preset_);
        jsonObject["presetText"] = presetToString(preset_);
        jsonObject["acState"] = static_cast<int>(acState_);

        String jsonString;
        serializeJson(jsonObject, jsonString);
        return jsonString;
    }

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

    // Helper functions to convert enum values to strings
    const char *modeToString(Mode mode) const
    {
        switch (mode)
        {
        case Mode::HeatCool:
            return "Heat/Cool";
        case Mode::Cool:
            return "Cool";
        case Mode::Heat:
            return "Heat";
        case Mode::Dry:
            return "Dry";
        case Mode::FanOnly:
            return "FanOnly";
        case Mode::Off:
            return "Off";
        default:
            return "Unknown";
        }
    }

    const char *fanSpeedToString(FanSpeed speed) const
    {
        switch (speed)
        {
        case FanSpeed::Auto:
            return "Auto";
        case FanSpeed::Speed1:
            return "1";
        case FanSpeed::Speed2:
            return "2";
        case FanSpeed::Speed3:
            return "3";
        case FanSpeed::Speed4:
            return "4";
        case FanSpeed::Speed5:
            return "5";
        default:
            return "Unknown";
        }
    }

    const char *swingModeToString(SwingMode mode) const
    {
        switch (mode)
        {
        case SwingMode::Auto:
            return "Auto";
        case SwingMode::Up:
            return "Up";
        case SwingMode::UpCenter:
            return "UpCenter";
        case SwingMode::Center:
            return "Center";
        case SwingMode::DownCenter:
            return "DownCenter";
        case SwingMode::Down:
            return "Down";
        default:
            return "Unknown";
        }
    }

    const char *presetToString(Preset preset) const
    {
        switch (preset)
        {
        case Preset::Normal:
            return "Normal";
        case Preset::Powerful:
            return "Powerful";
        case Preset::Quiet:
            return "Quiet";
        default:
            return "Unknown";
        }
    }
};

// // Example usage
// void setup() {
//     // Initialize your ESP32 and other setup code here...

//     // Create an instance of the AirConditioner class
//     AirConditioner ac;

//     // Set some data for the Air Conditioner
//     ac.setInternalTemperature(26.5);
//     ac.setExternalTemperature(30.0);
//     ac.setTargetTemperature(22.0);
//     ac.setMode(AirConditioner::Mode::Cool);
//     ac.setFanSpeed(AirConditioner::FanSpeed::Speed3);
//     ac.setSwingMode(AirConditioner::SwingMode::Center);
//     ac.setPreset(AirConditioner::Preset::Powerful);

//     // Retrieve and print the data
//     Serial.begin(9600);
//     Serial.println("Internal Temperature: " + String(ac.getInternalTemperature()) + " C");
//     Serial.println("External Temperature: " + String(ac.getExternalTemperature()) + " C");
//     Serial.println("Target Temperature: " + String(ac.getTargetTemperature()) + " C");
//     Serial.println("Mode: " + String(static_cast<int>(ac.getMode())));
//     Serial.println("Fan Speed: " + String(static_cast<int>(ac.getFanSpeed())));
//     Serial.println("Swing Mode: " + String(static_cast<int>(ac.getSwingMode())));
//     Serial.println("Preset: " + String(static_cast<int>(ac.getPreset())));
// }

// void loop() {
//     // Your main loop code here...
// }
