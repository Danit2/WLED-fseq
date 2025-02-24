#ifndef WEB_UI_MANAGER_H
#define WEB_UI_MANAGER_H

#include "wled.h"

// Trieda zodpovedná za registráciu web endpointov pre SD a FSEQ správu.
class WebUIManager {
  public:
    WebUIManager() {}
    void registerEndpoints();
};

#endif // WEB_UI_MANAGER_H