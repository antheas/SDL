/*
  Simple DirectMedia Layer
  Copyright (C) 2025 Mitchell Cairns <mitch.cairns@handheldlegend.com>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#ifdef SDL_JOYSTICK_HIDAPI

#include "../../SDL_hints_c.h"
#include "../SDL_sysjoystick.h"

#include "SDL_hidapijoystick_c.h"
#include "SDL_hidapi_rumble.h"
#include "SDL_hidapi_sinput.h"

#ifdef SDL_JOYSTICK_HIDAPI_SINPUT

/*****************************************************************************************************/
// This protocol is documented at:
// https://docs.handheldlegend.com/s/sinput/doc/sinput-hid-protocol-TkPYWlDMAg
/*****************************************************************************************************/

// Define this if you want to log all packets from the controller
#if 0
#define DEBUG_SINPUT_PROTOCOL
#endif

#if 0
#define DEBUG_SINPUT_INIT
#endif

#define SINPUT_DEVICE_REPORT_SIZE           64 // Size of input reports (And CMD Input reports)
#define SINPUT_DEVICE_REPORT_COMMAND_SIZE   48 // Size of command OUTPUT reports
#define SINPUT_DEVICE_REPORT_FEATURE_SIZE   64 // Size of feature reports

#define SINPUT_DEVICE_REPORT_ID_JOYSTICK_INPUT  0x01
#define SINPUT_DEVICE_REPORT_ID_FEAT            0x02
#define SINPUT_DEVICE_REPORT_ID_INPUT_CMDDAT    0x02
#define SINPUT_DEVICE_REPORT_ID_OUTPUT_CMDDAT   0x03

#define SINPUT_DEVICE_COMMAND_HAPTIC        0x01
#define SINPUT_DEVICE_COMMAND_FEATURES      0x02
#define SINPUT_DEVICE_COMMAND_PLAYERLED     0x03
#define SINPUT_DEVICE_COMMAND_JOYSTICKRGB   0x04

#define SINPUT_HAPTIC_TYPE_PRECISE          0x01
#define SINPUT_HAPTIC_TYPE_ERMSIMULATION    0x02

#define SINPUT_DEFAULT_GYRO_SENS  2000
#define SINPUT_DEFAULT_ACCEL_SENS 8

#define SINPUT_REPORT_IDX_BUTTONS_0         3
#define SINPUT_REPORT_IDX_BUTTONS_1         4
#define SINPUT_REPORT_IDX_BUTTONS_2         5
#define SINPUT_REPORT_IDX_BUTTONS_3         6
#define SINPUT_REPORT_IDX_LEFT_X            7
#define SINPUT_REPORT_IDX_LEFT_Y            9
#define SINPUT_REPORT_IDX_RIGHT_X           11
#define SINPUT_REPORT_IDX_RIGHT_Y           13
#define SINPUT_REPORT_IDX_LEFT_TRIGGER      15
#define SINPUT_REPORT_IDX_RIGHT_TRIGGER     17
#define SINPUT_REPORT_IDX_IMU_TIMESTAMP     19
#define SINPUT_REPORT_IDX_IMU_ACCEL_X       23
#define SINPUT_REPORT_IDX_IMU_ACCEL_Y       25
#define SINPUT_REPORT_IDX_IMU_ACCEL_Z       27
#define SINPUT_REPORT_IDX_IMU_GYRO_X        29
#define SINPUT_REPORT_IDX_IMU_GYRO_Y        31
#define SINPUT_REPORT_IDX_IMU_GYRO_Z        33
#define SINPUT_REPORT_IDX_TOUCH1_X          35
#define SINPUT_REPORT_IDX_TOUCH1_Y          37
#define SINPUT_REPORT_IDX_TOUCH1_P          39
#define SINPUT_REPORT_IDX_TOUCH2_X          41
#define SINPUT_REPORT_IDX_TOUCH2_Y          43
#define SINPUT_REPORT_IDX_TOUCH2_P          45

#define SINPUT_BUTTON_IDX_SOUTH             0
#define SINPUT_BUTTON_IDX_EAST              1
#define SINPUT_BUTTON_IDX_WEST              2
#define SINPUT_BUTTON_IDX_NORTH             3
#define SINPUT_BUTTON_IDX_DPAD_UP           4
#define SINPUT_BUTTON_IDX_DPAD_DOWN         5
#define SINPUT_BUTTON_IDX_DPAD_LEFT         6
#define SINPUT_BUTTON_IDX_DPAD_RIGHT        7
#define SINPUT_BUTTON_IDX_LEFT_STICK        8
#define SINPUT_BUTTON_IDX_RIGHT_STICK       9
#define SINPUT_BUTTON_IDX_LEFT_BUMPER       10
#define SINPUT_BUTTON_IDX_RIGHT_BUMPER      11
#define SINPUT_BUTTON_IDX_LEFT_TRIGGER      12
#define SINPUT_BUTTON_IDX_RIGHT_TRIGGER     13
#define SINPUT_BUTTON_IDX_LEFT_PADDLE1      14
#define SINPUT_BUTTON_IDX_RIGHT_PADDLE1     15
#define SINPUT_BUTTON_IDX_START             16
#define SINPUT_BUTTON_IDX_BACK              17
#define SINPUT_BUTTON_IDX_GUIDE             18
#define SINPUT_BUTTON_IDX_CAPTURE           19
#define SINPUT_BUTTON_IDX_LEFT_PADDLE2      20
#define SINPUT_BUTTON_IDX_RIGHT_PADDLE2     21
#define SINPUT_BUTTON_IDX_TOUCHPAD1         22
#define SINPUT_BUTTON_IDX_TOUCHPAD2         23
#define SINPUT_BUTTON_IDX_POWER             24
#define SINPUT_BUTTON_IDX_MISC4             25
#define SINPUT_BUTTON_IDX_MISC5             26
#define SINPUT_BUTTON_IDX_MISC6             27
#define SINPUT_BUTTON_IDX_MISC7             28
#define SINPUT_BUTTON_IDX_MISC8             29
#define SINPUT_BUTTON_IDX_MISC9             30
#define SINPUT_BUTTON_IDX_MISC10            31

#define SINPUT_REPORT_IDX_COMMAND_RESPONSE_ID   1
#define SINPUT_REPORT_IDX_COMMAND_RESPONSE_BULK 2

#define SINPUT_REPORT_IDX_PLUG_STATUS     1
#define SINPUT_REPORT_IDX_CHARGE_LEVEL    2

#define SINPUT_MAX_ALLOWED_TOUCHPADS 2

#ifndef EXTRACTSINT16
#define EXTRACTSINT16(data, idx) ((Sint16)((data)[(idx)] | ((data)[(idx) + 1] << 8)))
#endif

#ifndef EXTRACTUINT16
#define EXTRACTUINT16(data, idx) ((Uint16)((data)[(idx)] | ((data)[(idx) + 1] << 8)))
#endif

#ifndef EXTRACTUINT32
#define EXTRACTUINT32(data, idx) ((Uint32)((data)[(idx)] | ((data)[(idx) + 1] << 8) | ((data)[(idx) + 2] << 16) | ((data)[(idx) + 3] << 24)))
#endif

typedef struct
{
    SDL_HIDAPI_Device *device;
    Uint16 protocol_version;
    bool sensors_enabled;

    Uint8 player_idx;

    bool player_leds_supported;
    bool joystick_rgb_supported;
    bool rumble_supported;
    bool accelerometer_supported;
    bool gyroscope_supported;
    bool left_analog_stick_supported;
    bool right_analog_stick_supported;
    bool left_analog_trigger_supported;
    bool right_analog_trigger_supported;
    bool dpad_supported;
    bool touchpad_supported;
    bool is_handheld;
    bool trigger_rumble_supported;

    Uint8 touchpad_count;        // 2 touchpads maximum
    Uint8 touchpad_finger_count; // 2 fingers for one touchpad, or 1 per touchpad (2 max)
    Uint8 touchpad_width_mm;     // Touchpad width in mm
    Uint8 touchpad_height_mm;    // Touchpad height in mm

    float polling_rate_hz;
    Uint8  subtype;
    ESinputControllerType controller_type;
    ESinputFaceStyle face_style;

    Uint16 accelRange; // Example would be 2,4,8,16 +/- (g-force)
    Uint16 gyroRange;  // Example would be 1000,2000,4000 +/- (degrees per second)

    float accelScale; // Scale factor for accelerometer values
    float gyroScale;  // Scale factor for gyroscope values
    Uint8 last_state[USB_PACKET_LENGTH];

    Uint8 buttons_count;
    Uint8 usage_masks[4];

    Uint32 last_imu_timestamp_us;
    Uint64 imu_timestamp_ns;

    Uint16 left_rumble;
    Uint16 right_rumble;
    Uint16 left_trigger_rumble;
    Uint16 right_trigger_rumble;
} SDL_DriverSInput_Context;

void HIDAPI_DriverSInput_GetControllerType(
    Uint16 vendor, Uint16 product, Uint16 version, Uint8 subtype, ESinputControllerType *controller_type, ESinputFaceStyle *face_style)
{
    // There is no decided global layout format yet
    *face_style = k_eSInputFaceStyle_ukwn;
    *controller_type = k_eSInputControllerType_FullMapping;

    if (vendor == USB_VENDOR_RASPBERRYPI && product == USB_PRODUCT_HANDHELDLEGEND_PROGCC) {
        *controller_type = k_eSInputControllerType_HHL_PROGCC;
        *face_style = k_eSInputFaceStyle_ukwn;
    } else if (vendor == USB_VENDOR_RASPBERRYPI && product == USB_PRODUCT_HANDHELDLEGEND_GCULTIMATE) {
        *controller_type = k_eSInputControllerType_HHL_GCCULT;
        *face_style = k_eSInputFaceStyle_ukwn;
    } else if (vendor == USB_VENDOR_RASPBERRYPI && product == USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC) {
        *face_style = (subtype & 0xE0) >> 5;
        // Defer layout selection for this pair until later
        *controller_type = k_eSInputControllerType_FullMapping;
    } else if (vendor == 0x16d0 && product == 0x145b) {
        // Initial virtual mapping type
        *face_style = (subtype & 0xE0) >> 5;
        *controller_type = (subtype & 0x1F);
    }
}

// Converts raw int16_t gyro scale setting
static inline float CalculateGyroScale(uint16_t dps_range)
{
    return SDL_PI_F / 180.0f / (32768.0f / (float)dps_range);
}

// Converts raw int16_t accel scale setting
static inline float CalculateAccelScale(uint16_t g_range)
{
    return SDL_STANDARD_GRAVITY / (32768.0f / (float)g_range);
}

static bool ProcessSDLFeaturesResponse(SDL_HIDAPI_Device *device, Uint8 *data)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;
    bool left_analog_stick_supported, right_analog_stick_supported,
         left_analog_trigger_supported, right_analog_trigger_supported;
    Uint8 *fflags, *buttons, *serial;
    SDL_GamepadType type;

    // Obtain protocol version
    ctx->protocol_version = EXTRACTUINT16(data, 0);

    switch (ctx->protocol_version) {
    case 2:
        // Byte    | Data     | Meaning
        // --------|----------|-------------------------------------------------------
        // 0-1     | Various  | Protocol Version (2)
        // 2       | 0x00–FF  | SDL Gamepad Type (See SDL Gamepad Type)
        // 3       | 0x00–FF  | SDL Gamepad GUID Metadata
        // 4-9     | Uint8x5  | MAC Address or Serial Number
        // 10-11   | Uint16   | Polling rate in Hz (engine hint; may vary. Use IMU timestamp for accuracy)
        // 12-13   | Uint16   | Accelerometer count per 10m/^2 (use 10197 for 10k/G)
        // 14-15   | Uint16   | Gyroscope count per 10rad/s (use 11465 for 20dps)
        // 16      | Uint8    | Touchpad Width (mm; up to 25.6 cm)
        // 17      | Uint8    | Touchpad Height (mm; up to 25.6 cm)
        // 18-21   | Uint8x4  | Button Usage Masks (See Buttons Format)
        // 22-26   | Various  | Feature Flags 1-5
        //
        // Feature Flags 1:
        // Same as V1/V0
        //
        // Feature Flags 2:
        // 0x0X: Touchpad Support
        // 0x10: Is Handheld
        // 0x20: Joystick RGB Support
        // 0x30: Trigger Rumble Support
        //
        // The rest is for future proofing
        //
        // Touchpad Support:
        // First two bits:
        // 0x00: No Support
        // 0x01: One Touchpad, 1 finger
        // 0x02: One Touchpad, 2 fingers
        // 0x03: Two Touchpads, 1 finger each
        //
        // 0x04: Reserved for other touchpad modes
        // 0x08: Real pressure support. If 0, pressure = certainty
        //       Use pressure = 1 to indicate touch without pressure

        //
        // Gamepad Info
        //
        type = SDL_GAMEPAD_TYPE_UNKNOWN;
        type = (SDL_GamepadType)SDL_clamp(data[2], SDL_GAMEPAD_TYPE_UNKNOWN, SDL_GAMEPAD_TYPE_COUNT);
        device->type = type;

        // The 3 MSB represent a button layout style SDL_GamepadFaceStyle
        // The 5 LSB represent a device sub-type
        device->guid.data[15] = data[3];
        ctx->subtype = data[3];

        // Get device Serial - MAC address
        serial = data + 4;

        //
        // IMU Info
        //
        ctx->polling_rate_hz = EXTRACTUINT16(data, 10);
        ctx->accelScale = 10.0f / EXTRACTUINT16(data, 12);
        ctx->gyroScale = 10.0f / EXTRACTUINT16(data, 14);

        //
        // Unpack feature flags into context
        //
        buttons = data + 18;
        fflags = data + 22;
        ctx->rumble_supported = (fflags[0] & 0x01) != 0;
        ctx->player_leds_supported = (fflags[0] & 0x02) != 0;
        ctx->accelerometer_supported = (fflags[0] & 0x04) != 0;
        ctx->gyroscope_supported = (fflags[0] & 0x08) != 0;

        // Axes cannot be dynamic, so we only sanity check them
        left_analog_stick_supported = (fflags[0] & 0x10) != 0;
        right_analog_stick_supported = (fflags[0] & 0x20) != 0;
        left_analog_trigger_supported = (fflags[0] & 0x40) != 0;
        right_analog_trigger_supported = (fflags[0] & 0x80) != 0;
        
        // Touchpad support
        ctx->touchpad_width_mm = data[16];
        ctx->touchpad_height_mm = data[17];
        switch (fflags[1] & 0x07) {
        case 0x01:
            ctx->touchpad_supported = true;
            ctx->touchpad_count = 1;
            ctx->touchpad_finger_count = 1;
            break;
        case 0x02:
            ctx->touchpad_supported = true;
            ctx->touchpad_count = 1;
            ctx->touchpad_finger_count = 2;
            break;
        case 0x03:
            ctx->touchpad_supported = true;
            ctx->touchpad_count = 2;
            ctx->touchpad_finger_count = 1;
            break;
        default:
        case 0x00:
            ctx->touchpad_supported = false;
            ctx->touchpad_count = 0;
            ctx->touchpad_finger_count = 0;
            break;
        }
        ctx->is_handheld = (fflags[1] & 0x10) != 0 || (type == SDL_GAMEPAD_TYPE_HANDHELD);
        ctx->joystick_rgb_supported = (fflags[1] & 0x20) != 0;
        ctx->trigger_rumble_supported = (fflags[1] & 0x30) != 0;
        break;
    case 1:
    case 0:
        // Byte    | Data     | Meaning
        // --------|----------|-------------------------------------------------------
        // 0-1     | Various  | Protocol Version (0-1)
        // 2       | Various  | Feature Flags 1 (See Features Response Bytes)
        // 3       | Various  | Feature Flags 2 (See Features Response Bytes)
        // 4       | 0x00–FF  | SDL Gamepad Type (See SDL Gamepad Type)
        // 5       | 0x00–FF  | SDL Gamepad GUID Metadata
        // 6       | Uint8    | Polling rate (Milliseconds)
        // 7       | Uint8    | Reserved
        // 8-9     | Uint16   | Accelerometer G force range
        // 10-11   | Uint16   | Gyroscope DPS sensitivity range
        // 12-15   | Uint8    | Button Usage Masks (See Buttons Format)
        // 16      | Uint8    | Touchpad Count (Max 2 touchpads)
        // 17      | Uint8    | Touchpad Finger Count (Max 2 fingers TOTAL)
        // 18-23   | Uint8    | MAC Address or Serial Number
        //
        // Feature Flags 1:
        // 0x01 - Rumble supported
        // 0x02 - Player LEDs supported
        // 0x04 - Accelerometer supported
        // 0x08 - Gyroscope supported
        // 0x10 - Left Analog Stick supported
        // 0x20 - Right Analog Stick supported
        // 0x40 - Left Analog Trigger supported
        // 0x80 - Right Analog Trigger supported
        //
        // Feature Flags 2:
        // 0x01 - Touchpad supported
        // 0x02 - Joystick RGB supported
        // 0x04 - Is Handheld

        //
        // Unpack feature flags into context
        //
        fflags = data + 2;
        buttons = data + 12;
        ctx->rumble_supported = (fflags[0] & 0x01) != 0;
        ctx->player_leds_supported = (fflags[0] & 0x02) != 0;
        ctx->accelerometer_supported = (fflags[0] & 0x04) != 0;
        ctx->gyroscope_supported = (fflags[0] & 0x08) != 0;

        // Axes cannot be dynamic, so we only sanity check them
        left_analog_stick_supported = (fflags[0] & 0x10) != 0;
        right_analog_stick_supported = (fflags[0] & 0x20) != 0;
        left_analog_trigger_supported = (fflags[0] & 0x40) != 0;
        right_analog_trigger_supported = (fflags[0] & 0x80) != 0;

        ctx->touchpad_supported = (fflags[1] & 0x01) != 0;
        ctx->joystick_rgb_supported = (fflags[1] & 0x02) != 0;
        ctx->is_handheld = (fflags[1] & 0x04) != 0;

        //
        // Gamepad Info
        //
        type = SDL_GAMEPAD_TYPE_UNKNOWN;
        type = (SDL_GamepadType)SDL_clamp(data[4], SDL_GAMEPAD_TYPE_UNKNOWN, SDL_GAMEPAD_TYPE_COUNT);
        device->type = type;

        // The 3 MSB represent a button layout style SDL_GamepadFaceStyle
        // The 5 LSB represent a device sub-type
        device->guid.data[15] = data[5];
        ctx->subtype = data[5];

        // Get and validate touchpad parameters
        ctx->touchpad_count = data[16];
        ctx->touchpad_finger_count = data[17];

        //
        // IMU Info
        //
        ctx->polling_rate_hz = 1000.0f / data[6];
        ctx->accelScale = CalculateAccelScale(EXTRACTUINT16(data, 8));
        ctx->gyroScale = CalculateGyroScale(EXTRACTUINT16(data, 10));

        // Get device Serial - MAC address
        serial = data + 18;
        break;
    default:
        SDL_SetError("SInput device protocol version %d is not supported", ctx->protocol_version);
        return false;
    }

    // Copy serial
    char serial_str[18];
    (void)SDL_snprintf(serial_str, sizeof(serial_str), "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x",
                       serial[0], serial[1], serial[2], serial[3], serial[4], serial[5]);
    HIDAPI_SetDeviceSerial(device, serial_str);

    //
    // Get mappings based on SDL subtype and assert that they match.
    //
    HIDAPI_DriverSInput_GetControllerType(
        device->vendor_id, device->product_id, device->version, ctx->subtype,
        &ctx->controller_type, &ctx->face_style);

    switch (ctx->controller_type) {
    case k_eSInputControllerType_XInputOnly:
    case k_eSInputControllerType_XInputShareNone:
    case k_eSInputControllerType_XInputShareDual:
    case k_eSInputControllerType_XInputShareQuad:
    case k_eSInputControllerType_XInputShareNoneClick:
    case k_eSInputControllerType_XInputShareDualClick:
    case k_eSInputControllerType_XInputShareQuadClick:
        // Basic xinput mask with share
        ctx->usage_masks[0] = 0xFF;
        ctx->usage_masks[1] = 0x0F;
        ctx->usage_masks[2] = 0x0F;
        ctx->usage_masks[3] = 0x00;
        ctx->left_analog_stick_supported = true;
        ctx->right_analog_stick_supported = true;
        ctx->left_analog_trigger_supported = true;
        ctx->right_analog_trigger_supported = true;

        // Add primary paddles
        if (
            ctx->controller_type == k_eSInputControllerType_XInputShareDual ||
            ctx->controller_type == k_eSInputControllerType_XInputShareQuad ||
            ctx->controller_type == k_eSInputControllerType_XInputShareDualClick ||
            ctx->controller_type == k_eSInputControllerType_XInputShareQuadClick)
            ctx->usage_masks[1] |= 0xC0;

        // Remove share/capture button if not supported
        if (ctx->controller_type == k_eSInputControllerType_XInputOnly)
            ctx->usage_masks[2] &= ~0x08;

        // Add secondary paddles
        if (
            ctx->controller_type == k_eSInputControllerType_XInputShareQuad ||
            ctx->controller_type == k_eSInputControllerType_XInputShareQuadClick)
            ctx->usage_masks[2] |= 0x30;

        // Add touchpad click
        if (
            ctx->controller_type == k_eSInputControllerType_XInputShareNoneClick ||
            ctx->controller_type == k_eSInputControllerType_XInputShareDualClick ||
            ctx->controller_type == k_eSInputControllerType_XInputShareQuadClick)
            ctx->usage_masks[2] |= 0x40;
        break;
    case k_eSInputControllerType_HHL_PROGCC:
    case k_eSInputControllerType_HHL_GCCULT:
    case k_eSInputControllerType_LoadFirmware:
        ctx->usage_masks[0] = buttons[0];
        ctx->usage_masks[1] = buttons[1];
        ctx->usage_masks[2] = buttons[2];
        ctx->usage_masks[3] = buttons[3];
        ctx->left_analog_stick_supported = left_analog_stick_supported;
        ctx->right_analog_stick_supported = right_analog_stick_supported;
        ctx->left_analog_trigger_supported = left_analog_trigger_supported;
        ctx->right_analog_trigger_supported = right_analog_trigger_supported;
        break;
    case k_eSInputControllerType_FullMapping:
    default:
        ctx->usage_masks[0] = 0xFF;
        ctx->usage_masks[1] = 0xFF;
        ctx->usage_masks[2] = 0xFF;
        ctx->usage_masks[3] = 0xFF;
        ctx->left_analog_stick_supported = true;
        ctx->right_analog_stick_supported = true;
        ctx->left_analog_trigger_supported = true;
        ctx->right_analog_trigger_supported = true;
        break;
    }

    // Since SDL uses fixed mappings, unfortunately we cannot use the
    // button mask from the protocol. SInput defines a set of predefined
    // sub-types for this use. However, we can check it matches our expectations.
    if (
        // Masks in LSB to MSB
        // South, East, West, North, DUp, DDown, DLeft, DRight
        ctx->usage_masks[0] != buttons[0] ||
        // Left Stick, Right Stick, L Shoulder, R Shoulder,
        // L Trigger, R Trigger, L Paddle 1, R Paddle 1
        ctx->usage_masks[1] != buttons[1] ||
        // Start, Back, Guide, Capture, L Paddle 2, R Paddle 2, Touchpad L, Touchpad R
        ctx->usage_masks[2] != buttons[2] ||
        // Power, Misc 4 to 10
        ctx->usage_masks[3] != buttons[3] ||
        // Check Axes
        ctx->left_analog_stick_supported != left_analog_stick_supported ||
        ctx->right_analog_stick_supported != right_analog_stick_supported ||
        ctx->left_analog_trigger_supported != left_analog_trigger_supported ||
        ctx->right_analog_trigger_supported != right_analog_trigger_supported
    ) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_INPUT,
            "SInput device %s has different button mask than controller type 0x%.2x or that type is uknown. Found: %.2x%.2x%.2x%.2x-%d%d%d%d and will use: %.2x%.2x%.2x%.2x-%d%d%d%d",
            device->name, ctx->controller_type,
            buttons[0], buttons[1], buttons[2], buttons[3],
            left_analog_stick_supported, right_analog_stick_supported,
            left_analog_trigger_supported, right_analog_trigger_supported,
            ctx->usage_masks[0], ctx->usage_masks[1], ctx->usage_masks[2], ctx->usage_masks[3],
            ctx->left_analog_stick_supported, ctx->right_analog_stick_supported,
            ctx->left_analog_trigger_supported, ctx->right_analog_trigger_supported
        );
    }

    // Derive button count from mask
    for (Uint8 byte = 0; byte < 4; ++byte) {
        for (Uint8 bit = 0; bit < 8; ++bit) {
            if ((ctx->usage_masks[byte] & (1 << bit)) != 0) {
                ++ctx->buttons_count;
            }
        }
    }

    // Convert DPAD to hat
    const int DPAD_MASK = (1 << SINPUT_BUTTON_IDX_DPAD_UP) |
                          (1 << SINPUT_BUTTON_IDX_DPAD_DOWN) |
                          (1 << SINPUT_BUTTON_IDX_DPAD_LEFT) |
                          (1 << SINPUT_BUTTON_IDX_DPAD_RIGHT);
    if ((ctx->usage_masks[0] & DPAD_MASK) == DPAD_MASK) {
        ctx->dpad_supported = true;
        ctx->usage_masks[0] &= ~DPAD_MASK;
        ctx->buttons_count -= 4;
    }

#if defined(DEBUG_SINPUT_INIT)
    SDL_Log("SInput Protocol Version: %d", ctx->protocol_version);
    SDL_Log("SInput Face Style: %d", (device->guid.data[15] & 0xE0) >> 5);
    SDL_Log("SInput Sub-type: %d", (device->guid.data[15] & 0x1F));
    SDL_Log("Buttons count: %d", ctx->buttons_count);
    SDL_Log("Serial num: %s", serial);
    SDL_Log("Accelerometer Scale: %f", ctx->accelScale);
    SDL_Log("Gyro Scale: %f", ctx->gyroScale);
#endif

    return true;
}

static bool RetrieveSDLFeaturesReport(SDL_HIDAPI_Device *device)
{
    int status = 0;

    unsigned char report[SINPUT_DEVICE_REPORT_FEATURE_SIZE] = {
        SINPUT_DEVICE_REPORT_ID_FEAT,
        SINPUT_DEVICE_COMMAND_FEATURES,
        'S', 'I', 'N', 'P', 'U', 'T', 0, 0,
    };
    // This write will occasionally return -1, so ignore failure here and try again
    status = SDL_hid_send_feature_report(device->dev, report, sizeof(report));

    if (status != sizeof(report)) {
#if defined(DEBUG_SINPUT_INIT)
        // Not all controllers support this. Let's not spam them.
        SDL_LogWarn(
            SDL_LOG_CATEGORY_INPUT,
            "SInput device did not respond to set feature request. Error: %d",
            status);
        return false;
#endif
    }

    status = SDL_hid_get_feature_report(device->dev, report, sizeof(report));

    if (status < 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_INPUT,
            "SInput device did not respond to get feature request. Error: %d",
            status
        );
        return false;
    }

    if (report[0] != SINPUT_DEVICE_REPORT_ID_FEAT ||
        report[1] != SINPUT_DEVICE_COMMAND_FEATURES ||
        report[2] != 'S' || report[3] != 'I') {
        HIDAPI_DumpPacket("SInput device did not respond with the expected feature report.", report, sizeof(report));
        return false;
    }

    return ProcessSDLFeaturesResponse(device, (Uint8 *) &report[2]);
}

static bool RetrieveSDLFeaturesPolling(SDL_HIDAPI_Device *device)
{
    int written = 0;

    // Attempt to send the SDL features get command.
    for (int attempt = 0; attempt < 8; ++attempt) {
        const Uint8 featuresGetCommand[SINPUT_DEVICE_REPORT_COMMAND_SIZE] = { SINPUT_DEVICE_REPORT_ID_OUTPUT_CMDDAT, SINPUT_DEVICE_COMMAND_FEATURES };
        // This write will occasionally return -1, so ignore failure here and try again
        written = SDL_hid_write(device->dev, featuresGetCommand, sizeof(featuresGetCommand));

        if (written == SINPUT_DEVICE_REPORT_COMMAND_SIZE) {
            break;
        }
    }

    if (written < SINPUT_DEVICE_REPORT_COMMAND_SIZE) {
        SDL_SetError("SInput device SDL Features GET command could not write");
        return false;
    }

    int read = 0;

    // Read the reply
    for (int i = 0; i < 100; ++i) {
        SDL_Delay(1);

        Uint8 data[USB_PACKET_LENGTH];
        read = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0);
        if (read < 0) {
            SDL_SetError("SInput device SDL Features GET command could not read");
            return false;
        }
        if (read == 0) {
            continue;
        }

#ifdef DEBUG_SINPUT_PROTOCOL
        HIDAPI_DumpPacket("SInput packet: size = %d", data, read);
#endif

        if ((read == USB_PACKET_LENGTH) && (data[0] == SINPUT_DEVICE_REPORT_ID_INPUT_CMDDAT) && (data[1] == SINPUT_DEVICE_COMMAND_FEATURES)) {
            return ProcessSDLFeaturesResponse(device, &(data[SINPUT_REPORT_IDX_COMMAND_RESPONSE_BULK]));
        }
    }

    return false;
}

static void HIDAPI_DriverSInput_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_SINPUT, callback, userdata);
}

static void HIDAPI_DriverSInput_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_SINPUT, callback, userdata);
}

static bool HIDAPI_DriverSInput_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_SINPUT, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT));
}

static bool HIDAPI_DriverSInput_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    return SDL_IsJoystickSInputController(vendor_id, product_id);
}

static bool HIDAPI_DriverSInput_InitDevice(SDL_HIDAPI_Device *device)
{
#if defined(DEBUG_SINPUT_INIT)
    SDL_Log("SInput device Init");
#endif

    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return false;
    }

    ctx->device = device;
    device->context = ctx;

    if (!RetrieveSDLFeaturesReport(device)
        && !RetrieveSDLFeaturesPolling(device))
        return false;

    switch (device->product_id) {
    case USB_PRODUCT_HANDHELDLEGEND_GCULTIMATE:
        HIDAPI_SetDeviceName(device, "HHL GC Ultimate");
        break;
    case USB_PRODUCT_HANDHELDLEGEND_PROGCC:
        HIDAPI_SetDeviceName(device, "HHL ProGCC");
        break;
    default:
        // Use the USB product name
        break;
    }

    return HIDAPI_JoystickConnected(device, NULL);
}

static int HIDAPI_DriverSInput_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverSInput_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    if (ctx->player_leds_supported) {
        player_index = SDL_clamp(player_index + 1, 0, 255);
        Uint8 player_num = (Uint8)player_index;

        ctx->player_idx = player_num;

        // Set player number, finalizing the setup
        Uint8 playerLedCommand[SINPUT_DEVICE_REPORT_COMMAND_SIZE] = { SINPUT_DEVICE_REPORT_ID_OUTPUT_CMDDAT, SINPUT_DEVICE_COMMAND_PLAYERLED, ctx->player_idx };
        int playerNumBytesWritten = SDL_hid_write(device->dev, playerLedCommand, SINPUT_DEVICE_REPORT_COMMAND_SIZE);

        if (playerNumBytesWritten < 0) {
            SDL_SetError("SInput device player led command could not write");
        }
    }
}

#ifndef DEG2RAD
#define DEG2RAD(x) ((float)(x) * (float)(SDL_PI_F / 180.f))
#endif


static bool HIDAPI_DriverSInput_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
#if defined(DEBUG_SINPUT_INIT)
    SDL_Log("SInput device Open");
#endif

    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    SDL_AssertJoysticksLocked();

    joystick->nbuttons = ctx->buttons_count;

    SDL_zeroa(ctx->last_state);

    int axes = 0;
    if (ctx->left_analog_stick_supported) {
        axes += 2;
    }

    if (ctx->right_analog_stick_supported) {
        axes += 2;
    }

    if (ctx->left_analog_trigger_supported) {
        ++axes;
    }

    if (ctx->right_analog_trigger_supported) {
        ++axes;
    }

    joystick->naxes = axes;

    if (ctx->dpad_supported) {
        joystick->nhats = 1;
    }

    if (ctx->accelerometer_supported) {
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, ctx->polling_rate_hz);
    }

    if (ctx->gyroscope_supported) {
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, ctx->polling_rate_hz);
    }

    if (ctx->touchpad_supported) {
        // If touchpad is supported, minimum 1, max is capped
        ctx->touchpad_count = SDL_clamp(ctx->touchpad_count, 1, SINPUT_MAX_ALLOWED_TOUCHPADS);

        if (ctx->touchpad_count > 1) {
            // Support two separate touchpads with 1 finger each
            // or support one touchpad with 2 fingers max
            ctx->touchpad_finger_count = 1;
        }

        if (ctx->touchpad_count > 0) {
            SDL_PrivateJoystickAddTouchpad(joystick, ctx->touchpad_finger_count);
        }

        if (ctx->touchpad_count > 1) {
            SDL_PrivateJoystickAddTouchpad(joystick, ctx->touchpad_finger_count);
        }
    }

    return true;
}

static void HIDAPI_DriverSInput_UpdateRumble(SDL_HIDAPI_Device *device)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    Uint8 report[SINPUT_DEVICE_REPORT_COMMAND_SIZE] = {
        SINPUT_DEVICE_REPORT_ID_OUTPUT_CMDDAT,
        SINPUT_DEVICE_COMMAND_HAPTIC,
    };

    switch (ctx->protocol_version) {
    case 1:
    case 0:
        // THese are simple. Only 8 bit rumble
        report[2] = 0x02;
        report[3] = (Uint8)(ctx->left_rumble >> 8);
        report[4] = 0;
        report[5] = (Uint8)(ctx->right_rumble >> 8);
        report[6] = 0;
        break;
    case 2:
    default:
        report[2] = (Uint8)(ctx->left_rumble >> 8);
        report[3] = (Uint8)(ctx->left_rumble & 0xFF);
        report[4] = (Uint8)(ctx->left_trigger_rumble >> 8);
        report[5] = (Uint8)(ctx->left_trigger_rumble & 0xFF);
        report[6] = (Uint8)(ctx->right_rumble >> 8);
        report[7] = (Uint8)(ctx->right_rumble & 0xFF);
        report[8] = (Uint8)(ctx->right_trigger_rumble >> 8);
        report[9] = (Uint8)(ctx->right_trigger_rumble & 0xFF);
        break;
    }
    SDL_HIDAPI_SendRumble(device, report, SINPUT_DEVICE_REPORT_COMMAND_SIZE);
}

static bool HIDAPI_DriverSInput_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    if (ctx->rumble_supported) {
        ctx->left_rumble = low_frequency_rumble;
        ctx->right_rumble = high_frequency_rumble;
        HIDAPI_DriverSInput_UpdateRumble(device);
        return true;
    }

    return SDL_Unsupported();
}

static bool HIDAPI_DriverSInput_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    if (ctx->trigger_rumble_supported) {
        ctx->left_trigger_rumble = left_rumble;
        ctx->right_trigger_rumble = right_rumble;
        HIDAPI_DriverSInput_UpdateRumble(device);
        return true;
    }

    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverSInput_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    Uint32 caps = 0;
    if (ctx->rumble_supported) {
        caps |= SDL_JOYSTICK_CAP_RUMBLE;
    }

    if (ctx->trigger_rumble_supported) {
        caps |= SDL_JOYSTICK_CAP_TRIGGER_RUMBLE;
    }

    if (ctx->player_leds_supported) {
        caps |= SDL_JOYSTICK_CAP_PLAYER_LED;
    }

    if (ctx->joystick_rgb_supported) {
        caps |= SDL_JOYSTICK_CAP_RGB_LED;
    }

    return caps;
}

static bool HIDAPI_DriverSInput_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    if (ctx->joystick_rgb_supported) {
        Uint8 joystickRGBCommand[SINPUT_DEVICE_REPORT_COMMAND_SIZE] = { SINPUT_DEVICE_REPORT_ID_OUTPUT_CMDDAT, SINPUT_DEVICE_COMMAND_JOYSTICKRGB, red, green, blue };
        int joystickRGBBytesWritten = SDL_hid_write(device->dev, joystickRGBCommand, SINPUT_DEVICE_REPORT_COMMAND_SIZE);

        if (joystickRGBBytesWritten < 0) {
            SDL_SetError("SInput device joystick rgb command could not write");
            return false;
        }

        return true;
    }
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSInput_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSInput_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    if (ctx->accelerometer_supported || ctx->gyroscope_supported) {
        ctx->sensors_enabled = enabled;
        return true;
    }
    return SDL_Unsupported();
}

static void HIDAPI_DriverSInput_HandleStatePacket(SDL_Joystick *joystick, SDL_DriverSInput_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis = 0;
    Sint16 accel = 0;
    Sint16 gyro = 0;
    Uint64 timestamp = SDL_GetTicksNS();
    float imu_values[3] = { 0 };
    Uint8 output_idx = 0;

    // Process digital buttons according to the supplied
    // button mask to create a contiguous button input set
    for (Uint8 processes = 0; processes < 4; ++processes) {

        Uint8 button_idx = SINPUT_REPORT_IDX_BUTTONS_0 + processes;

        for (Uint8 buttons = 0; buttons < 8; ++buttons) {

            // If a button is enabled by our usage mask
            const Uint8 mask = (0x01 << buttons);
            if ((ctx->usage_masks[processes] & mask) != 0) {

                bool down = (data[button_idx] & mask) != 0;

                if ( (output_idx < SDL_GAMEPAD_BUTTON_COUNT) && (ctx->last_state[button_idx] != data[button_idx]) ) {
                    SDL_SendJoystickButton(timestamp, joystick, output_idx, down);
                }

                ++output_idx;
            }
        }
    }

    if (ctx->dpad_supported) {
        Uint8 hat = SDL_HAT_CENTERED;

        if (data[SINPUT_REPORT_IDX_BUTTONS_0] & (1 << SINPUT_BUTTON_IDX_DPAD_UP)) {
            hat |= SDL_HAT_UP;
        }
        if (data[SINPUT_REPORT_IDX_BUTTONS_0] & (1 << SINPUT_BUTTON_IDX_DPAD_DOWN)) {
            hat |= SDL_HAT_DOWN;
        }
        if (data[SINPUT_REPORT_IDX_BUTTONS_0] & (1 << SINPUT_BUTTON_IDX_DPAD_LEFT)) {
            hat |= SDL_HAT_LEFT;
        }
        if (data[SINPUT_REPORT_IDX_BUTTONS_0] & (1 << SINPUT_BUTTON_IDX_DPAD_RIGHT)) {
            hat |= SDL_HAT_RIGHT;
        }
        SDL_SendJoystickHat(timestamp, joystick, 0, hat);
    }

    // Analog inputs map to a signed Sint16 range of -32768 to 32767 from the device.
    // Use an axis index because not all gamepads will have the same axis inputs.
    Uint8 axis_idx = 0;

    // Left Analog Stick
    axis = 0; // Reset axis value for joystick
    if (ctx->left_analog_stick_supported) {
        axis = EXTRACTSINT16(data, SINPUT_REPORT_IDX_LEFT_X);
        SDL_SendJoystickAxis(timestamp, joystick, axis_idx, axis);
        ++axis_idx;

        axis = EXTRACTSINT16(data, SINPUT_REPORT_IDX_LEFT_Y);
        SDL_SendJoystickAxis(timestamp, joystick, axis_idx, axis);
        ++axis_idx;
    }

    // Right Analog Stick
    axis = 0; // Reset axis value for joystick
    if (ctx->right_analog_stick_supported) {
        axis = EXTRACTSINT16(data, SINPUT_REPORT_IDX_RIGHT_X);
        SDL_SendJoystickAxis(timestamp, joystick, axis_idx, axis);
        ++axis_idx;

        axis = EXTRACTSINT16(data, SINPUT_REPORT_IDX_RIGHT_Y);
        SDL_SendJoystickAxis(timestamp, joystick, axis_idx, axis);
        ++axis_idx;
    }

    // Left Analog Trigger
    axis = SDL_MIN_SINT16; // Reset axis value for trigger
    if (ctx->left_analog_trigger_supported) {
        axis = EXTRACTSINT16(data, SINPUT_REPORT_IDX_LEFT_TRIGGER);
        SDL_SendJoystickAxis(timestamp, joystick, axis_idx, axis);
        ++axis_idx;
    }

    // Right Analog Trigger
    axis = SDL_MIN_SINT16; // Reset axis value for trigger
    if (ctx->right_analog_trigger_supported) {
        axis = EXTRACTSINT16(data, SINPUT_REPORT_IDX_RIGHT_TRIGGER);
        SDL_SendJoystickAxis(timestamp, joystick, axis_idx, axis);
    }

    // Battery/Power state handling
    if (ctx->last_state[SINPUT_REPORT_IDX_PLUG_STATUS]  != data[SINPUT_REPORT_IDX_PLUG_STATUS] ||
        ctx->last_state[SINPUT_REPORT_IDX_CHARGE_LEVEL] != data[SINPUT_REPORT_IDX_CHARGE_LEVEL]) {

        SDL_PowerState state = SDL_POWERSTATE_UNKNOWN;
        Uint8 status = data[SINPUT_REPORT_IDX_PLUG_STATUS];
        int percent = data[SINPUT_REPORT_IDX_CHARGE_LEVEL];

        percent = SDL_clamp(percent, 0, 100); // Ensure percent is within valid range

        switch (status) {
        case 1:
            state = SDL_POWERSTATE_NO_BATTERY;
            percent = 0;
            break;
        case 2:
            state = SDL_POWERSTATE_CHARGING;
            break;
        case 3:
            state = SDL_POWERSTATE_CHARGED;
            percent = 100;
            break;
        case 4:
            state = SDL_POWERSTATE_ON_BATTERY;
            break;
        default:
            break;
        }

        if (state != SDL_POWERSTATE_UNKNOWN) {
            SDL_SendJoystickPowerInfo(joystick, state, percent);
        }
    }

    // Extract the IMU timestamp delta (in microseconds)
    Uint32 imu_timestamp_us = EXTRACTUINT32(data, SINPUT_REPORT_IDX_IMU_TIMESTAMP);
    Uint32 imu_time_delta_us = 0;

    // Check if we should process IMU data and if sensors are enabled
    if (ctx->sensors_enabled) {

        if (imu_timestamp_us >= ctx->last_imu_timestamp_us) {
            imu_time_delta_us = (imu_timestamp_us - ctx->last_imu_timestamp_us);
        } else {
            // Handle rollover case
            imu_time_delta_us = (UINT32_MAX - ctx->last_imu_timestamp_us) + imu_timestamp_us + 1;
        }

        // Convert delta to nanoseconds and update running timestamp
        ctx->imu_timestamp_ns += (Uint64)imu_time_delta_us * 1000;

        // Update last timestamp
        ctx->last_imu_timestamp_us = imu_timestamp_us;

        // Process Accelerometer
        if (ctx->accelerometer_supported) {

            accel = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_ACCEL_Y);
            imu_values[2] = -(float)accel * ctx->accelScale; // Y-axis acceleration

            accel = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_ACCEL_Z);
            imu_values[1] = (float)accel * ctx->accelScale; // Z-axis acceleration

            accel = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_ACCEL_X);
            imu_values[0] = -(float)accel * ctx->accelScale; // X-axis acceleration

            SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL, ctx->imu_timestamp_ns, imu_values, 3);
        }

        // Process Gyroscope
        if (ctx->gyroscope_supported) {

            gyro = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_GYRO_Y);
            imu_values[2] = -(float)gyro * ctx->gyroScale; // Y-axis rotation

            gyro = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_GYRO_Z);
            imu_values[1] = (float)gyro * ctx->gyroScale; // Z-axis rotation

            gyro = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_GYRO_X);
            imu_values[0] = -(float)gyro * ctx->gyroScale; // X-axis rotation

            SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO, ctx->imu_timestamp_ns, imu_values, 3);
        }
    }

    // Check if we should process touchpad
    if (ctx->touchpad_supported && ctx->touchpad_count > 0) {
        Uint8 touchpad = 0;
        Uint8 finger = 0;

        Sint16 touch1X = EXTRACTSINT16(data, SINPUT_REPORT_IDX_TOUCH1_X);
        Sint16 touch1Y = EXTRACTSINT16(data, SINPUT_REPORT_IDX_TOUCH1_Y);
        Uint16 touch1P = EXTRACTUINT16(data, SINPUT_REPORT_IDX_TOUCH1_P);

        Sint16 touch2X = EXTRACTSINT16(data, SINPUT_REPORT_IDX_TOUCH2_X);
        Sint16 touch2Y = EXTRACTSINT16(data, SINPUT_REPORT_IDX_TOUCH2_Y);
        Uint16 touch2P = EXTRACTUINT16(data, SINPUT_REPORT_IDX_TOUCH2_P);

        SDL_SendJoystickTouchpad(timestamp, joystick, touchpad, finger,
            touch1P > 0,
            touch1X / 65536.0f + 0.5f,
            touch1Y / 65536.0f + 0.5f,
            touch1P / 32768.0f);

        if (ctx->touchpad_count > 1) {
            ++touchpad;
        } else if (ctx->touchpad_finger_count > 1) {
            ++finger;
        }

        if ((touchpad > 0) || (finger > 0)) {
            SDL_SendJoystickTouchpad(timestamp, joystick, touchpad, finger,
                                     touch2P > 0,
                                     touch2X / 65536.0f + 0.5f,
                                     touch2Y / 65536.0f + 0.5f,
                                     touch2P / 32768.0f);
        }
    }

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}

static bool HIDAPI_DriverSInput_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size = 0;

    if (device->num_joysticks > 0) {
        joystick = SDL_GetJoystickFromID(device->joysticks[0]);
    } else {
        return false;
    }

    while ((size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_SINPUT_PROTOCOL
        HIDAPI_DumpPacket("SInput packet: size = %d", data, size);
#endif
        if (!joystick) {
            continue;
        }

        // Handle command response information
        if (data[0] == SINPUT_DEVICE_REPORT_ID_JOYSTICK_INPUT) {
            HIDAPI_DriverSInput_HandleStatePacket(joystick, ctx, data, size);
        }
    }

    if (size < 0) {
        // Read error, device is disconnected
        HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
    }
    return (size >= 0);
}

static void HIDAPI_DriverSInput_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
}

static void HIDAPI_DriverSInput_FreeDevice(SDL_HIDAPI_Device *device)
{
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSInput = {
    SDL_HINT_JOYSTICK_HIDAPI_SINPUT,
    true,
    HIDAPI_DriverSInput_RegisterHints,
    HIDAPI_DriverSInput_UnregisterHints,
    HIDAPI_DriverSInput_IsEnabled,
    HIDAPI_DriverSInput_IsSupportedDevice,
    HIDAPI_DriverSInput_InitDevice,
    HIDAPI_DriverSInput_GetDevicePlayerIndex,
    HIDAPI_DriverSInput_SetDevicePlayerIndex,
    HIDAPI_DriverSInput_UpdateDevice,
    HIDAPI_DriverSInput_OpenJoystick,
    HIDAPI_DriverSInput_RumbleJoystick,
    HIDAPI_DriverSInput_RumbleJoystickTriggers,
    HIDAPI_DriverSInput_GetJoystickCapabilities,
    HIDAPI_DriverSInput_SetJoystickLED,
    HIDAPI_DriverSInput_SendJoystickEffect,
    HIDAPI_DriverSInput_SetJoystickSensorsEnabled,
    HIDAPI_DriverSInput_CloseJoystick,
    HIDAPI_DriverSInput_FreeDevice,
};

#endif // SDL_JOYSTICK_HIDAPI_SINPUT

#endif // SDL_JOYSTICK_HIDAPI
