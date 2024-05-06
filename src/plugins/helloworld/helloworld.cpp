#include "VPXPlugin.h"

VPXPluginAPI* vpx = nullptr;

void onGameStart(const unsigned int eventId, void* data)
{
   // Game is starting (plugin can be loaded and kept alive through multiple game plays)
   // After this event, all functions of the API marked as 'in game only' can be called
}

void onGameEnd(const unsigned int eventId, void* data)
{
   // Game is ending
   // After this event, all functions of the API marked as 'in game only' may not be called anymore
}

void onPrepareFrame(const unsigned int eventId, void* data)
{
   // Called when the player is about to prepare a new frame
   // This can be used ot tweak any visual parameter before building the frame (for example head tracking,...)
}

VPX_EXPORT bool PluginLoad(VPXPluginAPI* api)
{
   vpx = api;
   // We must check that the provided API is the one used to compile this plugin and fail otherwise
   if (vpx->version != VPX_PLUGIN_API_VERSION)
      return false;
   vpx->SubscribeEvent(vpx->GetEventID(VPX_EVT_ON_GAME_START), onGameStart);
   vpx->SubscribeEvent(vpx->GetEventID(VPX_EVT_ON_GAME_END), onGameEnd);
   vpx->SubscribeEvent(vpx->GetEventID(VPX_EVT_ON_PREPARE_FRAME), onPrepareFrame);
   return true;
}

VPX_EXPORT void PluginUnload()
{
   // Cleanup is mandatory when plugin is unloaded. All registered callbacks must be unregistered.
   vpx->UnsubscribeEvent(vpx->GetEventID(VPX_EVT_ON_GAME_START), onGameStart);
   vpx->UnsubscribeEvent(vpx->GetEventID(VPX_EVT_ON_GAME_END), onGameEnd);
   vpx->UnsubscribeEvent(vpx->GetEventID(VPX_EVT_ON_PREPARE_FRAME), onPrepareFrame);
   vpx = nullptr;
}
