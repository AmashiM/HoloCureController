// for those paranoid people at home here's the source code
#include <Windows.h>
#include <Xinput.h>
#include <math.h>
#include <stdio.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <tuple>

#pragma comment(lib, "XInput.lib")
#pragma comment(lib, "Xinput9_1_0.lib")

using std::cout;
using std::div;
using std::endl;
using std::map;
using std::min;
using std::string;
using namespace std::chrono;
using std::wstring;

#define KEYEVENTF_KEYDOWN \
  0x0000  // Technically this constant doesn't exist in the MS documentation.
          // It's the lack of KEYEVENTF_KEYUP that means pressing the key down.
#define KEYEVENTF_KEYUP 0x0002
#define X_DEADZONE 0.1
#define Y_DEADZONE 0.1

struct KEYINPUT {
  WORD wVk;
  WORD wScan;
  DWORD dwFlags;
  DWORD time;
  ULONG* dwExtraInfo;
};

uint64_t ms() {
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
      .count();
}

void key_down(BYTE key) { keybd_event(key, 0, KEYEVENTF_KEYDOWN, 0); };

void key_up(BYTE key) { keybd_event(key, 0, KEYEVENTF_KEYUP, 0); };

void key_press(BYTE key) {
  keybd_event(key, 0, KEYEVENTF_KEYDOWN, 0);
  keybd_event(key, 0, KEYEVENTF_KEYUP, 0);
};

struct Key {
  BYTE key;
  bool request = false;
  bool move = false;
  int req = 0;
  Key(BYTE key) { this->key = key; }

  void up() { key_up(key); }

  void down() { key_down(key); }

  void press() { key_press(key); }

  void handle(bool condition) {
    if (condition) {
      key_down(key);
      request = true;
    } else if (request) {
      key_up(key);
      request = false;
    }
  }

  void handlePress(bool condition) {
    if (condition) {
      if (!request) key_down(key);
      request = true;
    } else if (request) {
      key_up(key);
      request = false;
    }
  }

  void direction(bool condition, Key opposite) {
    if (condition) {
      key_down(key);
      if (req == 0) {
        key_down(opposite.key);
      }
      req = 1;
    } else if (req == 1) {
      key_up(opposite.key);
      key_up(key);
      req = 0;
    }
  }
};

class XboxController {
 private:
  int id;

 public:
  XINPUT_STATE state;
  XINPUT_GAMEPAD gamepad;
  WORD wButtons;

  XboxController(int id) {
    this->id = id;
    GetState();
  }
  ~XboxController() {}

  void GetState() {
    ZeroMemory(&state, sizeof(XINPUT_STATE));

    XInputGetState(id, &state);
    gamepad = state.Gamepad;
    wButtons = gamepad.wButtons;
  }

  bool IsConnected() {
    ZeroMemory(&state, sizeof(XINPUT_STATE));

    DWORD Result = XInputGetState(id, &state);

    if (Result == ERROR_SUCCESS) {
      gamepad = state.Gamepad;
      wButtons = gamepad.wButtons;
      return true;
    } else {
      return false;
    }
  }

  void Vibrate(int leftVal = 0, int rightVal = 0) {
    XINPUT_VIBRATION Vibration;

    ZeroMemory(&Vibration, sizeof(XINPUT_VIBRATION));

    Vibration.wLeftMotorSpeed = leftVal;
    Vibration.wRightMotorSpeed = rightVal;

    XInputSetState(id, &Vibration);
  }
};

namespace HoloCureControl {
double MAX_JOY_VAL = 32758;

namespace keys {
Key left(VK_LEFT);
Key right(VK_RIGHT);
Key up(VK_UP);
Key down(VK_DOWN);
Key escape(VK_ESCAPE);
Key enter(VK_RETURN);
Key z('Z');
Key x('X');
}  // namespace keys
};  // namespace HoloCureControl

using HoloCureControl::MAX_JOY_VAL;

double joy_value(double num) {
  return (num < MAX_JOY_VAL ? num : MAX_JOY_VAL) / MAX_JOY_VAL;
}

using namespace HoloCureControl::keys;

std::tuple<bool, HWND> get_holocure_window() {
  HWND holocure_window;
  bool ok = false;

  for (HWND hwnd = GetTopWindow(NULL); hwnd != NULL;
       hwnd = GetNextWindow(hwnd, GW_HWNDNEXT)) {
    int length = GetWindowTextLength(hwnd);
    if (length == 0) continue;
    const DWORD TITLE_SIZE = 1024;
    WCHAR windowTitle[TITLE_SIZE];

    GetWindowTextW(hwnd, windowTitle, TITLE_SIZE);

    wstring title(&windowTitle[0]);

    if (title == L"Program Manager" || title == L"Default IME" ||
        title == L"MSCTFIME UI")
      continue;

    if (title == L"HoloCure") {
      holocure_window = hwnd;
      ok = true;
      break;
    }
  }

  return std::make_tuple(ok, holocure_window);
}

int main(int argc, char* argv[]) {
  int cid = -1;
  DWORD dwResult;
  for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
    XINPUT_STATE state;
    ZeroMemory(&state, sizeof(XINPUT_STATE));

    dwResult = XInputGetState(i, &state);

    if (dwResult == ERROR_SUCCESS) {
      cid = i;
      break;
    }
  }

  if (cid == -1) {
    cout << "No Controller Connected." << endl;
    std::cin.get();
    return 0;
  } else {
    cout << "Controller Found." << endl;
  }

  std::tuple<bool, HWND> window_result = get_holocure_window();
  HWND holocure_window = std::get<1>(window_result);

  if (!std::get<0>(window_result)) {
    std::cout << "No Holocure Window Found." << endl;
    std::cin.get();
    return 0;
  }

  SetForegroundWindow(holocure_window);

  XboxController joy(0);

  int control_mode = 0;

  bool needs_reconnect = false;

  while (true) {
    // is connected will also update the state
    if (!joy.IsConnected()) {
      if (!needs_reconnect) {
        std::cout << "\a";
        std::cout
            << "No Controller is Connected. Please Connect Your Controller."
            << endl;
        escape.press();
        needs_reconnect = true;
      }
      Sleep(50);
      continue;
    } else if (needs_reconnect) {
      std::cout << "Controller Connected";
      needs_reconnect = false;
    }
    if (joy.wButtons & XINPUT_GAMEPAD_BACK) {
      break;
    }

    if (joy.wButtons & XINPUT_GAMEPAD_START) {
      joy.Vibrate(10000, 10000);
      if (control_mode == 0) {
        control_mode = 1;
      } else {
        control_mode = 0;
      }
      Sleep(15);
      joy.Vibrate();
      Sleep(25);
    }

    if (control_mode == 0) {
      double lx = joy_value(joy.gamepad.sThumbLX);
      double ly = joy_value(joy.gamepad.sThumbLY);
      double rx = joy_value(joy.gamepad.sThumbRX);
      double ry = joy_value(joy.gamepad.sThumbRY);

      // cout << "x: " << lx << "\n" << "y: " << ly << endl;

      right.handle(lx >= 0.65);
      left.handle(lx <= -0.65);
      up.handle(ly >= 0.65);
      down.handle(ly <= -0.65);
    } else {
      right.handlePress(joy.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
      left.handlePress(joy.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
      up.handlePress(joy.wButtons & XINPUT_GAMEPAD_DPAD_UP);
      down.handlePress(joy.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
    }

    z.handlePress(joy.wButtons & XINPUT_GAMEPAD_A);
    escape.handlePress(joy.wButtons & XINPUT_GAMEPAD_B);
    x.handlePress(joy.wButtons & XINPUT_GAMEPAD_X);
    enter.handlePress(joy.wButtons & XINPUT_GAMEPAD_Y);

    Sleep(15);
  }

  return 0;
}
