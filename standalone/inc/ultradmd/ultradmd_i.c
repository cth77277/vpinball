/*** Autogenerated by WIDL 10.0-rc4 from UltraDMD.idl - Do not edit ***/

#include <rpc.h>
#include <rpcndr.h>

#ifdef _MIDL_USE_GUIDDEF_

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
    DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)

#elif defined(__cplusplus)

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
    EXTERN_C const type DECLSPEC_SELECTANY name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#else

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
    const type DECLSPEC_SELECTANY name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#endif

#ifdef __cplusplus
extern "C" {
#endif

MIDL_DEFINE_GUID(IID, LIBID_UltraDMD, 0x30b5ccd9, 0x5104, 0x41a8, 0xa6,0x97, 0x2a,0x9e,0x86,0x1f,0xbc,0x2a);
MIDL_DEFINE_GUID(IID, IID_IDMDObject, 0xf7e68187, 0x251f, 0x4dfb, 0xaf,0x79, 0xf1,0xd4,0xd6,0x9e,0xe1,0x88);
MIDL_DEFINE_GUID(CLSID, CLSID_DMDObject, 0xe1612654, 0x304a, 0x4e07, 0xa2,0x36, 0xeb,0x64,0xd6,0xd4,0xf5,0x11);
MIDL_DEFINE_GUID(IID, DIID_IDMDObjectEvents, 0x0decff48, 0x5492, 0x43e7, 0xab,0x6c, 0xbf,0xd9,0x24,0x5f,0x2e,0xad);

#ifdef __cplusplus
}
#endif

#undef MIDL_DEFINE_GUID
