

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Tue Jan 19 08:14:07 2038
 */
/* Compiler settings for MyInterface.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.01.0628 
    protocol : all , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */



/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */


#ifndef __MyInterface_h_h__
#define __MyInterface_h_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef DECLSPEC_XFGVIRT
#if defined(_CONTROL_FLOW_GUARD_XFG)
#define DECLSPEC_XFGVIRT(base, func) __declspec(xfg_virtual(base, func))
#else
#define DECLSPEC_XFGVIRT(base, func)
#endif
#endif

/* Forward Declarations */ 

/* header files for imported files */
#include "ms-dtyp.h"

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __MyRpcInterface_INTERFACE_DEFINED__
#define __MyRpcInterface_INTERFACE_DEFINED__

/* interface MyRpcInterface */
/* [unique][version][uuid] */ 

DWORD Ping( 
    /* [in] */ handle_t hBinding,
    /* [string][out] */ wchar_t **response);

DWORD Execute( 
    /* [in] */ handle_t hBinding,
    /* [string][in] */ wchar_t *input,
    /* [string][out] */ wchar_t **output);

DWORD ExecuteSilent( 
    /* [in] */ handle_t hBinding,
    /* [string][in] */ wchar_t *input);

DWORD UploadFile( 
    /* [in] */ handle_t hBinding,
    /* [string][in] */ wchar_t *remotePath,
    /* [in] */ DWORD fileSize,
    /* [size_is][in] */ byte fileData[  ]);

DWORD DownloadFile( 
    /* [in] */ handle_t hBinding,
    /* [string][in] */ wchar_t *remotePath,
    /* [out] */ DWORD *fileSize,
    /* [size_is][size_is][out] */ byte **fileData);

DWORD ListDirectory( 
    /* [in] */ handle_t hBinding,
    /* [string][in] */ wchar_t *path,
    /* [string][out] */ wchar_t **output);

VOID ShutDown( 
    /* [in] */ handle_t hBinding);



extern RPC_IF_HANDLE MyRpcInterface_v1_0_c_ifspec;
extern RPC_IF_HANDLE MyRpcInterface_v1_0_s_ifspec;
#endif /* __MyRpcInterface_INTERFACE_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


