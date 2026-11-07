#ifndef HOT_STREAMING_PATCHSTATUSUI_H
#define HOT_STREAMING_PATCHSTATUSUI_H

// The patch status panel and the login gate while the downloader is busy.
namespace HoT::Streaming::PatchStatusUI {

bool ClientUIMissing();

// Runs the built in status UI with state "frame" or "glue" when Interface\SharedXML\StreamingPatchStatus.lua is missing.
void Load(const char* state);

// Called first thing in Script_DefaultServerLogin, true when the login was refused and the script must return.
bool BlockLogin();

}

#endif
