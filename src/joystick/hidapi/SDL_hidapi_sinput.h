/*
  Simple DirectMedia Layer
  Copyright (C) 2025 Antheas Kapenekakis <git@antheas.dev>

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

// Layouts define the available buttons and axes of the controller and
// are used to generate the SDL mapping string.
// Xinput means all Xbox360 buttons.
// Xinput_share adds a share button, and then dual, quad add 2, 4 paddles
// to those. Click adds a touchpad click. Touchpad is not used in the name
// because not all of them have clicks. In fact, there are no controllers
// with dual touchpads w/ clicks so there is no point in adding them. Yet.
typedef enum
{
    k_eSInputControllerType_FullMapping = 0x00,
    k_eSInputControllerType_XInputOnly = 0x01,
    k_eSInputControllerType_XInputShareNone = 0x02,
    k_eSInputControllerType_XInputShareDual = 0x03,
    k_eSInputControllerType_XInputShareQuad = 0x04,
    k_eSInputControllerType_XInputShareNoneClick = 0x05,
    k_eSInputControllerType_XInputShareDualClick = 0x06,
    k_eSInputControllerType_XInputShareQuadClick = 0x07,
    k_eSInputControllerType_HHL_PROGCC = 0xffff0100,
    k_eSInputControllerType_HHL_GCCULT = 0xffff0101,
    k_eSInputControllerType_LoadFirmware = 0xffffffff,
} ESinputControllerType;

typedef enum
{
    k_eSInputFaceStyle_ukwn = 0x00,
    k_eSInputFaceStyle_abxy = 0x01,
    k_eSInputFaceStyle_axby = 0x02,
    k_eSInputFaceStyle_bayx = 0x03,
    k_eSInputFaceStyle_sony = 0x04,
} ESinputFaceStyle;

extern void HIDAPI_DriverSInput_GetControllerType(
    Uint16 vendor, Uint16 product, Uint16 version, Uint8 subtype, ESinputControllerType *controller_type, ESinputFaceStyle *face_style);