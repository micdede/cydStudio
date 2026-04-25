#include "settings.h"

#include <Preferences.h>

namespace cydstudio::settings {

static const char* NS = "cydsettings";

int get_rotation() {
  Preferences p; p.begin(NS, true);
  int r = p.getInt("rot", 0);
  p.end();
  return r & 3;
}

void set_rotation(int q) {
  Preferences p; p.begin(NS, false);
  p.putInt("rot", q & 3);
  p.end();
}

}  // namespace cydstudio::settings
