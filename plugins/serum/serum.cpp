// license:GPLv3+

#include <cassert>
#include <cstdlib>
#include <chrono>
#include <cstdlib>

#include "MsgPlugin.h"
#include "CorePlugin.h"
#include "PinMamePlugin.h"
#include "libserum/serum-decode.h"

///////////////////////////////////////////////////////////////////////////////
// Serum Colorization plugin
//
// This plugin only rely on the generic message plugin API and the following
// messages:
// - PinMame/onGameStart: msgData is PinMame game identifier (rom name)
// - PinMame/onGameEnd
// - Controller/GetDMD: msgData is a request/response struct

MsgPluginAPI* msgApi = nullptr;
unsigned int endpointId, getDmdSrcId, getRenderDmdId, getIdentifyDmdId, onGameStartId, onGameEndId, onDmdTrigger;

Serum_Frame_Struc* pSerum;
unsigned int dmdId, lastRawFrameId;
bool dmdSelected = false;

class ColorizationState
{
public:
   ColorizationState(unsigned int width, unsigned int height)
      : m_width(width), m_height(height), m_size(width * height)
      , m_colorFrame(pSerum->SerumVersion == SERUM_V1 ? new uint8_t[width * height * 3] : nullptr)
      , m_colorizedFrameFormat(pSerum->SerumVersion == SERUM_V1 ? CTLPI_GETDMD_FORMAT_SRGB888 : CTLPI_GETDMD_FORMAT_SRGB565)
   {
      assert(m_width > 0);
      assert(m_height > 0);
      m_colorizedframeId = static_cast<unsigned int>(std::rand());
   }

   ~ColorizationState()
   {
      delete[] m_colorFrame;
   }

   void UpdateFrameV1()
   {
      assert(pSerum && (pSerum->SerumVersion == SERUM_V1));
      for (unsigned int i = 0; i < m_size; i++)
         memcpy(&(m_colorFrame[i * 3]), &pSerum->palette[pSerum->frame[i] * 3], 3);
      m_colorizedframeId++;
   }
   void UpdateFrame32V2()
   {
      assert(pSerum && (pSerum->SerumVersion == SERUM_V2));
      if (pSerum->width32 > 0)
      {
         m_width32 = pSerum->width32;
         m_colorFrame32 = reinterpret_cast<uint8_t*>(pSerum->frame32);
         m_colorizedframeId++;
      }
   }
   void UpdateFrame64V2()
   {
      assert(pSerum && (pSerum->SerumVersion == SERUM_V2));
      if (pSerum->width64 > 0)
      {
         m_width64 = pSerum->width64;
         m_colorFrame64 = reinterpret_cast<uint8_t*>(pSerum->frame64);
         m_colorizedframeId++;
      }
   }

   // Serum v1
   uint8_t* m_colorFrame;

   // Serum v2
   unsigned int m_width32 = 0;
   uint8_t* m_colorFrame32 = nullptr;
   unsigned int m_width64 = 0;
   uint8_t* m_colorFrame64 = nullptr;
   
   // Common state informations
   const unsigned int m_width, m_height, m_size;
   const unsigned int m_colorizedFrameFormat;
   
   unsigned int m_colorizedframeId = 0;
   bool m_hasAnimation = false;
   std::chrono::high_resolution_clock::time_point m_animationTick;
   std::chrono::high_resolution_clock::time_point m_animationNextTick;
};

ColorizationState* state = nullptr;


void onGetIdentifyDMD(const unsigned int eventId, void* userData, void* msgData)
{
   assert(pSerum);
   GetDmdMsg* const getDmdMsg = static_cast<GetDmdMsg*>(msgData);

   // Only process selected DMD
   if (!dmdSelected || getDmdMsg->dmdId != dmdId)
      return;
   
   // Only process response with a new frame
   if ((getDmdMsg->frame == nullptr) || (getDmdMsg->frameId == lastRawFrameId))
      return;
   
   // We received a raw frame to identify/colorize (eventually requested by us)
   const uint32_t firstrot = Serum_Colorize(getDmdMsg->frame);
   lastRawFrameId = getDmdMsg->frameId;
   if (pSerum->triggerID != 0xffffffff)
      msgApi->BroadcastMsg(endpointId, onDmdTrigger, &pSerum->triggerID);
   if (firstrot != IDENTIFY_NO_FRAME)
   { // New frame, eventually starting a new animation
      if (state == nullptr)
         state = new ColorizationState(getDmdMsg->width, getDmdMsg->height);
      else if (state->m_width != getDmdMsg->width || state->m_height != getDmdMsg->height)
      {
         delete state;
         state = new ColorizationState(getDmdMsg->width, getDmdMsg->height);
      }
      
      state->m_hasAnimation = firstrot != 0;
      if (state->m_hasAnimation)
      {
         state->m_animationTick = std::chrono::high_resolution_clock::now();
         state->m_animationNextTick = state->m_animationTick + std::chrono::milliseconds(firstrot);
      }
      if (pSerum->SerumVersion == SERUM_V1)
         state->UpdateFrameV1();
      else if (pSerum->SerumVersion == SERUM_V2)
      {
         if (pSerum->flags & FLAG_RETURNED_32P_FRAME_OK)
            state->UpdateFrame32V2();
         if (pSerum->flags & FLAG_RETURNED_64P_FRAME_OK)
            state->UpdateFrame64V2();
      }
   }
}

void onGetRenderDMD(const unsigned int eventId, void* userData, void* msgData)
{
   assert(pSerum);
   GetDmdMsg& getDmdMsg = *static_cast<GetDmdMsg*>(msgData);

   // If main DMD not yet selected then do it
   if (!dmdSelected)
   {
      if (getDmdMsg.width < 128)
         return;
      dmdId = getDmdMsg.dmdId;
      dmdSelected = true;
   }

   // Only process selected DMD
   if (getDmdMsg.dmdId != dmdId)
      return;
   
   // Does someone requested a DMD frame to render that no one has colorized yet ?
   if ((getDmdMsg.frame != nullptr) && (getDmdMsg.format != CTLPI_GETDMD_FORMAT_LUM8))
      return;
   
   // Update to the last 'raw' frame
   GetDmdMsg getRawDmdMsg;
   memset(&getRawDmdMsg, 0, sizeof(getRawDmdMsg));
   getRawDmdMsg.dmdId = dmdId;
   msgApi->BroadcastMsg(endpointId, getIdentifyDmdId, &getRawDmdMsg);
   onGetIdentifyDMD(getIdentifyDmdId, nullptr, static_cast<void*>(&getRawDmdMsg));
   if (state == nullptr)
      return;
   
   // Perform current animation (catching up to the current time point)
   if (state->m_hasAnimation)
   {
      const auto now = std::chrono::high_resolution_clock::now();
      while (state->m_animationNextTick < now)
      {
         const uint32_t nextrot = Serum_Rotate();
         const uint32_t delayMs = nextrot & 0x0000ffff;
         if (delayMs == 0)
         {
            state->m_hasAnimation = false;
            break;
         }
         state->m_animationTick = state->m_animationNextTick;
         state->m_animationNextTick = state->m_animationNextTick + std::chrono::milliseconds(delayMs);
         if ((pSerum->SerumVersion == SERUM_V1) && (nextrot & FLAG_RETURNED_V1_ROTATED))
            state->UpdateFrameV1();
         if ((pSerum->SerumVersion == SERUM_V2) && (nextrot & FLAG_RETURNED_V2_ROTATED32))
            state->UpdateFrame32V2();
         if ((pSerum->SerumVersion == SERUM_V2) && (nextrot & FLAG_RETURNED_V2_ROTATED64))
            state->UpdateFrame64V2();
      }
   }

   // Answer to requester with last colorized frame (matching fixed size if requested)
   int frameSrc = -1;
   if (getDmdMsg.requestFlags & CTLPI_GETDMD_FLAG_RENDER_SIZE_REQ)
   {
      if ((getDmdMsg.height == 32) && (getDmdMsg.width == state->m_width32) && (state->m_colorFrame32))
         frameSrc = 2;
      else if ((getDmdMsg.height == 64) && (getDmdMsg.width == state->m_width64) && (state->m_colorFrame64))
         frameSrc = 3;
      else if ((getDmdMsg.height == state->m_height) && (getDmdMsg.width == state->m_width) && (state->m_colorFrame))
         frameSrc = 1;
   }
   else
   {
      if (state->m_colorFrame32)
         frameSrc = ((state->m_colorFrame64 == nullptr) || (state->m_height <= 32)) ? 2 : 3;
      else if (state->m_colorFrame64)
         frameSrc = 3;
      else if (state->m_colorFrame)
         frameSrc = 1;
   }
   
   if (frameSrc == -1)
      return;
   
   getDmdMsg.frameId = state->m_colorizedframeId;
   getDmdMsg.format = state->m_colorizedFrameFormat;
   switch (frameSrc)
   {
   case 1: // Serum V1
      getDmdMsg.frame = state->m_colorFrame;
      getDmdMsg.width = state->m_width;
      getDmdMsg.height = state->m_height;
      break;
   case 2: // Serum V2 Height 32
      getDmdMsg.frame = state->m_colorFrame32;
      getDmdMsg.width = state->m_width32;
      getDmdMsg.height = 32;
      break;
   case 3: // Serum V2 Height 64
      getDmdMsg.frame = state->m_colorFrame64;
      getDmdMsg.width = state->m_width64;
      getDmdMsg.height = 64;
      break;
   }
}

void onGetRenderDMDSrc(const unsigned int eventId, void* userData, void* msgData)
{
   if (pSerum == nullptr || state == nullptr || !dmdSelected)
      return;
   
   GetDmdSrcMsg& msg = *static_cast<GetDmdSrcMsg*>(msgData);
   if (state->m_colorFrame32 && state->m_width32 && msg.count < msg.maxEntryCount)
   {
      msg.entries[msg.count].dmdId = dmdId;
      msg.entries[msg.count].format = CTLPI_GETDMD_FORMAT_SRGB565;
      msg.entries[msg.count].width = state->m_width32;
      msg.entries[msg.count].height = 32;
      msg.count++;
   }
   if (state->m_colorFrame64 && state->m_width64 && msg.count < msg.maxEntryCount)
   {
      msg.entries[msg.count].dmdId = dmdId;
      msg.entries[msg.count].format = CTLPI_GETDMD_FORMAT_SRGB565;
      msg.entries[msg.count].width = state->m_width64;
      msg.entries[msg.count].height = 64;
      msg.count++;
   }
   if (state->m_colorFrame && state->m_width && state->m_height && msg.count < msg.maxEntryCount)
   {
      msg.entries[msg.count].dmdId = dmdId;
      msg.entries[msg.count].format = CTLPI_GETDMD_FORMAT_SRGB888;
      msg.entries[msg.count].width = state->m_width;
      msg.entries[msg.count].height = state->m_height;
      msg.count++;
   }
}

void onGameStart(const unsigned int eventId, void* userData, void* msgData)
{
   // Setup Serum on the selected DMD
   const char* gameId = static_cast<const char*>(msgData);
   assert(gameId != nullptr);
   char crzFolder[512];
   msgApi->GetSetting("serum.dmd", "crz_folder", crzFolder, sizeof(crzFolder));
   pSerum = Serum_Load(crzFolder, gameId, FLAG_REQUEST_32P_FRAMES | FLAG_REQUEST_64P_FRAMES);
   dmdSelected = false;
   if (pSerum)
   {
      msgApi->SubscribeMsg(endpointId, getDmdSrcId, onGetRenderDMDSrc, nullptr);
      msgApi->SubscribeMsg(endpointId, getRenderDmdId, onGetRenderDMD, nullptr);
      msgApi->SubscribeMsg(endpointId, getIdentifyDmdId, onGetIdentifyDMD, nullptr);
   }
}

void onGameEnd(const unsigned int eventId, void* userData, void* msgData)
{
   if (pSerum)
   {
      delete state;
      state = nullptr;
      msgApi->UnsubscribeMsg(getDmdSrcId, onGetRenderDMDSrc);
      msgApi->UnsubscribeMsg(getRenderDmdId, onGetRenderDMD);
      msgApi->UnsubscribeMsg(getIdentifyDmdId, onGetIdentifyDMD);
      Serum_Dispose();
      pSerum = nullptr;
   }
}

MSGPI_EXPORT void PluginLoad(const unsigned int sessionId, MsgPluginAPI* api)
{
   msgApi = api;
   endpointId = sessionId;
   getDmdSrcId = msgApi->GetMsgID(CTLPI_NAMESPACE, CTLPI_GETDMD_SRC_MSG);
   getRenderDmdId = msgApi->GetMsgID(CTLPI_NAMESPACE, CTLPI_GETDMD_RENDER_MSG);
   getIdentifyDmdId = msgApi->GetMsgID(CTLPI_NAMESPACE, CTLPI_GETDMD_IDENTIFY_MSG);
   onDmdTrigger = msgApi->GetMsgID("Serum", "OnDmdTrigger");
   msgApi->SubscribeMsg(sessionId, onGameStartId = msgApi->GetMsgID(PMPI_NAMESPACE, PMPI_EVT_ON_GAME_START), onGameStart, nullptr);
   msgApi->SubscribeMsg(sessionId, onGameEndId = msgApi->GetMsgID(PMPI_NAMESPACE, PMPI_EVT_ON_GAME_END), onGameEnd, nullptr);
}

MSGPI_EXPORT void PluginUnload()
{
   msgApi->UnsubscribeMsg(onGameStartId, onGameStart);
   msgApi->UnsubscribeMsg(onGameEndId, onGameEnd);
   msgApi->ReleaseMsgID(onGameStartId);
   msgApi->ReleaseMsgID(onGameEndId);
   msgApi->ReleaseMsgID(onDmdTrigger);
   msgApi->ReleaseMsgID(getDmdSrcId);
   msgApi->ReleaseMsgID(getRenderDmdId);
   msgApi->ReleaseMsgID(getIdentifyDmdId);
   msgApi = nullptr;
}
