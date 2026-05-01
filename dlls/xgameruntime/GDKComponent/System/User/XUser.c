/*
 * Copyright 2026 Olivia Ryan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/*
 * Xbox Game runtime Library
 * GDK Component: System API -> XUser
 */

#include "XUser.h"
#include "winhttp.h"

WINE_DEFAULT_DEBUG_CHANNEL(gdkc);

static const struct IXUserImplVtbl x_user_vtbl;
static const struct IXUserGamertagVtbl x_user_gt_vtbl;

#undef TRACE
#define TRACE FIXME

static HRESULT LoadDefaultUser( XUserHandle *user, LPCSTR client_id )
{
    FIXME( "LoadDefaultUser %d\n", 0 );
    // abort();
    struct x_user *impl;
    LSTATUS status;
    LPSTR buffer;
    HRESULT hr;
    DWORD size;

    if (!user) return E_POINTER;

    FIXME( "LoadDefaultUser %d\n", 1 );

    if (!(impl = calloc( 1, sizeof( *impl ) )))
    {
        free( buffer );
        return E_OUTOFMEMORY;
    }

    impl->IXUserImpl_iface.lpVtbl = &x_user_vtbl;
    impl->IXUserGamertag_iface.lpVtbl = &x_user_gt_vtbl;
    impl->ref = 1;
    // TODO: xuid
    impl->xuid = 0x0;

    *user = (XUserHandle)impl;

    FIXME( "LoadDefaultUser C\n" );

    return 0;
}

static inline struct x_user *impl_from_IXUserImpl( IXUserImpl *iface )
{
    return CONTAINING_RECORD( iface, struct x_user, IXUserImpl_iface );
}

static HRESULT WINAPI x_user_QueryInterface( IXUserImpl *iface, REFIID iid, void **out )
{
    struct x_user *impl = impl_from_IXUserImpl( iface );

    TRACE( "iface %p, iid %s, out %p\n", iface, debugstr_guid( iid ), out );

    if (!out) return E_POINTER;

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IXUserBase ) ||
        IsEqualGUID( iid, &IID_IXUserAddWithUi ) ||
        IsEqualGUID( iid, &IID_IXUserMsa ) ||
        IsEqualGUID( iid, &IID_IXUserStore ) ||
        IsEqualGUID( iid, &IID_IXUserPlatform ) ||
        IsEqualGUID( iid, &IID_IXUserSignOut ))
    {
        *out = &impl->IXUserImpl_iface;
        IXUserImpl_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IXUserGamertag ))
    {
        *out = &impl->IXUserGamertag_iface;
        IXUserGamertag_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI x_user_AddRef( IXUserImpl *iface )
{
    struct x_user *impl = impl_from_IXUserImpl( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu\n", iface, ref );
    return ref;
}

static ULONG WINAPI x_user_Release( IXUserImpl *iface )
{
    struct x_user *impl = impl_from_IXUserImpl( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu\n", iface, ref );
    if (!ref)
    {
        WindowsDeleteString( impl->refresh_token );
        WindowsDeleteString( impl->oauth_token );
        WindowsDeleteString( impl->user_token );
        WindowsDeleteString( impl->xsts_token );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI x_user_XUserDuplicateHandle( IXUserImpl *iface, XUserHandle user, XUserHandle *duplicated )
{
    TRACE( "iface %p, user %p, duplicated %p\n", iface, user, duplicated );
    if (!user || !duplicated) return E_POINTER;
    IXUserImpl_AddRef( &((struct x_user*)user)->IXUserImpl_iface );
    *duplicated = user;
    return S_OK;
}

static void WINAPI x_user_XUserCloseHandle( IXUserImpl *iface, XUserHandle user )
{
    TRACE( "iface %p, user %p\n", iface, user );
    if (user) IXUserImpl_Release( &((struct x_user*)user)->IXUserImpl_iface );
}

static INT32 WINAPI x_user_XUserCompare( IXUserImpl *iface, XUserHandle user1, XUserHandle user2 )
{
    TRACE( "iface %p, user1 %p, user2 %p\n", iface, user1, user2 );
    if (!user1 || !user2) return 1;
    return ((struct x_user*)user1)->xuid != ((struct x_user*)user2)->xuid;
}

static HRESULT WINAPI x_user_XUserGetMaxUsers( IXUserImpl *iface, UINT32 *maxUsers )
{
    FIXME( "iface %p, maxUsers %p stub!\n", iface, maxUsers );
    if(maxUsers) {
        *maxUsers = 1;
    }
    return 0;
}

struct XUserAddContext
{
    XUserAddOptions options;
    XUserHandle user;
    LPCSTR client_id;
};

static HRESULT XUserAddProvider( XAsyncOp operation, const XAsyncProviderData *providerData )
{
    struct XUserAddContext *context;
    IXThreadingImpl *impl;
    HRESULT hr;

    TRACE( "operation %d, providerData %p\n", operation, providerData );

    if (!providerData) return E_POINTER;
    if (FAILED( QueryApiImpl( &CLSID_XThreadingImpl, &IID_IXThreadingImpl, (void**)&impl ) )) return E_FAIL;
    context = providerData->context;

    switch (operation)
    {
        case Begin:
            context->user = NULL;
            return impl->lpVtbl->XAsyncSchedule( impl, providerData->async, 0 );

        case GetResult:
            if(context->user == NULL) {
                abort();
            }
            memcpy( providerData->buffer, &context->user, sizeof( XUserHandle ) );
            break;

        case DoWork:
            hr = LoadDefaultUser( &context->user, context->client_id );

            impl->lpVtbl->XAsyncComplete( impl, providerData->async, hr, sizeof( XUserHandle ) );
            break;

        case Cleanup:
            free( context );
            break;

        case Cancel:
            break;
    }

    return S_OK;
}

static HRESULT WINAPI x_user_XUserAddAsync( IXUserImpl *iface, XUserAddOptions options, XAsyncBlock *asyncBlock )
{
    struct XUserAddContext *context;
    IXThreadingImpl *impl;
    HRESULT hr;

    TRACE( "iface %p, options %d, asyncBlock %p\n", iface, options, asyncBlock );

    if (!asyncBlock) return E_POINTER;
    if (FAILED( hr = QueryApiImpl( &CLSID_XThreadingImpl, &IID_IXThreadingImpl, (void**)&impl ) )) return hr;
    if (!(context = calloc( 1, sizeof( struct XUserAddContext ) )))
    {
        impl->lpVtbl->Release( impl );
        return E_OUTOFMEMORY;
    }

    context->options = options;
    hr = impl->lpVtbl->XAsyncBegin( impl, asyncBlock, context, x_user_XUserAddAsync, "XUserAddAsync", XUserAddProvider );
    impl->lpVtbl->Release( impl );
    return hr;
}

static HRESULT WINAPI x_user_XUserAddResult( IXUserImpl *iface, XAsyncBlock *asyncBlock, XUserHandle *user )
{
    IXThreadingImpl *impl;

    TRACE( "iface %p, asyncBlock %p, user %p\n", iface, asyncBlock, user );

    if (!asyncBlock || !user) return E_POINTER;
    if (FAILED( QueryApiImpl( &CLSID_XThreadingImpl, &IID_IXThreadingImpl, (void**)&impl ) )) abort();
    return impl->lpVtbl->XAsyncGetResult( impl, asyncBlock, x_user_XUserAddAsync, sizeof( XUserHandle ), user, NULL );
}

static HRESULT WINAPI x_user_XUserGetLocalId( IXUserImpl *iface, XUserHandle user, XUserLocalId *localId )
{
    TRACE( "iface %p, user %p, localId %p\n", iface, user, localId );
    if (!user || !localId) return E_POINTER;
    XUserLocalId id;
    id.value = 1;
    *localId = id;
    return S_OK;
}

static HRESULT WINAPI x_user_XUserFindUserByLocalId( IXUserImpl *iface, XUserLocalId localId, XUserHandle *user )
{
    abort();
    FIXME( "iface %p, localId %p, user %p stub!\n", iface, &localId, user );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetId( IXUserImpl *iface, XUserHandle user, UINT64 *userId )
{
    TRACE( "iface %p, user %p, userId %p\n", iface, user, userId );
    if (!user || !userId) return E_POINTER;
    *userId = ((struct x_user*)user)->xuid;
    return S_OK;
}

static HRESULT WINAPI x_user_XUserFindUserById( IXUserImpl *iface, UINT64 userId, XUserHandle *user )
{
    abort();
    FIXME( "iface %p, userId %llu, user %p stub!\n", iface, userId, user );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetIsGuest( IXUserImpl *iface, XUserHandle user, BOOLEAN *isGuest )
{
    FIXME( "iface %p, user %p, isGuest %p stub!\n", iface, user, isGuest );
    if (!user || !isGuest) return E_POINTER;
    *isGuest = FALSE;
    return S_OK;
}

static HRESULT WINAPI x_user_XUserGetState( IXUserImpl *iface, XUserHandle user, XUserState *state )
{
    FIXME( "iface %p, user %p, state %p stub!\n", iface, user, state );
    abort();
    return E_NOTIMPL;
}

static HRESULT WINAPI __PADDING__( IXUserImpl *iface )
{
    WARN( "iface %p padding function called! It's unknown what this function does\n", iface );
    abort();
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetGamerPictureAsync( IXUserImpl *iface, XUserHandle user, XUserGamerPictureSize size, XAsyncBlock *asyncBlock )
{
    FIXME( "iface %p, user %p, size %p, asyncBlock %p stub!\n", iface, user, &size, asyncBlock );
    abort();
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetGamerPictureResultSize( IXUserImpl *iface, XAsyncBlock *asyncBlock, SIZE_T *size )
{
    FIXME( "iface %p, asyncBlock %p, size %p stub!\n", iface, asyncBlock, size );
    abort();
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetGamerPictureResult( IXUserImpl *iface, XAsyncBlock *asyncBlock, SIZE_T size, PVOID buffer, SIZE_T *used )
{
    FIXME( "iface %p, asyncBlock %p, size %llu, buffer %p, used %p stub!\n", iface, asyncBlock, size, buffer, used );
    abort();
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetAgeGroup( IXUserImpl *iface, XUserHandle user, XUserAgeGroup *group )
{
    TRACE( "iface %p, user %p, group %p\n", iface, user, group );

    if (!user || !group) return E_POINTER;
    *group = XUserAgeGroup_Adult;
    return S_OK;
}

static HRESULT WINAPI x_user_XUserCheckPrivilege( IXUserImpl *iface, XUserHandle user, XUserPrivilegeOptions options, XUserPrivilege privilege, BOOLEAN *hasPrivilege, XUserPrivilegeDenyReason *reason )
{
    FIXME( "iface %p, user %p, options %d, privilege %d, hasPrivilege %p, reason %p stub!\n", iface, user, options, privilege, hasPrivilege, reason );
    //abort();
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserResolvePrivilegeWithUiAsync( IXUserImpl *iface, XUserHandle user, XUserPrivilegeOptions options, XUserPrivilege privilege, XAsyncBlock *asyncBlock )
{
    FIXME( "iface %p, user %p, options %d, privilege %d, asyncBlock %p stub!\n", iface, user, options, privilege, asyncBlock );
    abort();
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserResolvePrivilegeWithUiResult( IXUserImpl *iface, XAsyncBlock *asyncBlock )
{
    FIXME( "iface %p, asyncBlock %p stub!\n", iface, asyncBlock );
    abort();
    return E_NOTIMPL;
}

struct XUserGetTokenAndSignatureContext
{
    BOOLEAN utf16;
    XUserHandle user;
    XUserGetTokenAndSignatureOptions options;
    LPCSTR method;
    LPCWSTR method_utf16;
    LPCSTR url;
    LPCWSTR url_utf16;
    SIZE_T count;
    XUserGetTokenAndSignatureHttpHeader *headers;
    XUserGetTokenAndSignatureUtf16HttpHeader *headers_utf16;
    SIZE_T size;
    const void *buffer;
};

static HRESULT XUserGetTokenAndSignatureProvider( XAsyncOp operation, const XAsyncProviderData *providerData )
{
    struct XUserGetTokenAndSignatureContext *context;
    IXThreadingImpl *impl;

    TRACE( "operation %d, providerData %p\n", operation, providerData );

    if (!providerData) return E_POINTER;
    if (FAILED( QueryApiImpl( &CLSID_XThreadingImpl, &IID_IXThreadingImpl, (void**)&impl ) )) return E_FAIL;
    context = providerData->context;

    switch (operation)
    {
        case Begin:
            return impl->lpVtbl->XAsyncSchedule( impl, providerData->async, 0 );

        case GetResult:
            break;

        case DoWork:
            impl->lpVtbl->XAsyncComplete( impl, providerData->async, S_OK, sizeof( XUserHandle ) );
            break;

        case Cleanup:
            if (context->count)
            {
                if (context->utf16) free( context->headers_utf16 );
                else free( context->headers );
            }
            free( context );
            break;

        case Cancel:
            break;
    }

    return S_OK;
}

static HRESULT WINAPI x_user_XUserGetTokenAndSignatureAsync( IXUserImpl *iface, XUserHandle user, XUserGetTokenAndSignatureOptions options, LPCSTR method, LPCSTR url, SIZE_T count, const XUserGetTokenAndSignatureHttpHeader *headers, SIZE_T size, const void *buffer, XAsyncBlock *asyncBlock )
{
    struct XUserGetTokenAndSignatureContext *context;
    IXThreadingImpl *impl;
    HRESULT hr;

    FIXME( "iface %p, user %p, options %d, method %s, url %s, count %llu, headers %p, size %llu, buffer %p, asyncBlock %p\n", iface, user, options, method, url, count, headers, size, buffer, asyncBlock );
    for(int i = 0; i < count; i++) {
        FIXME("%s: %s", headers[i].name, headers[i].value);
    }

    if (!user || !method || !url || !headers || !buffer || !asyncBlock) return E_POINTER;
    if (FAILED( hr = QueryApiImpl( &CLSID_XThreadingImpl, &IID_IXThreadingImpl, (void**)&impl ) )) return hr;
    if (!(context = calloc( 1, sizeof( *context ) )))
    {
        impl->lpVtbl->Release( impl );
        return E_OUTOFMEMORY;
    }

    context->options = options;
    context->buffer = buffer;
    context->method = method;
    context->count = count;
    context->utf16 = FALSE;
    context->size = size;
    context->user = user;
    context->url = url;
    if (count && !(context->headers = calloc( count, sizeof( *headers ) )))
    {
        free( context );
        impl->lpVtbl->Release( impl );
        return E_OUTOFMEMORY;
    }

    for (SIZE_T i = 0; i < count; i++)
        context->headers[i] = headers[i];

    hr = impl->lpVtbl->XAsyncBegin( impl, asyncBlock, context, x_user_XUserGetTokenAndSignatureAsync, "XUserGetTokenAndSignatureAsync", XUserGetTokenAndSignatureProvider );
    impl->lpVtbl->Release( impl );
    return hr;
}

static char tkn[] = "XBL3.0 x=;";

static HRESULT WINAPI x_user_XUserGetTokenAndSignatureResultSize( IXUserImpl *iface, XAsyncBlock *asyncBlock, SIZE_T *size )
{
    *size = sizeof(XUserGetTokenAndSignatureData) + sizeof(tkn);
    FIXME( "iface %p, asyncBlock %p, size %p stub!\n", iface, asyncBlock, size );
    return 0;
}

static HRESULT WINAPI x_user_XUserGetTokenAndSignatureResult( IXUserImpl *iface, XAsyncBlock *asyncBlock, SIZE_T size, PVOID buffer, XUserGetTokenAndSignatureData **ptr, SIZE_T *used )
{
    
    FIXME( "iface %p, asyncBlock %p, size %llu, buffer %p, ptr %p, used %p stub!\n", iface, asyncBlock, size, buffer, ptr, used );

    *ptr = (XUserGetTokenAndSignatureData*)buffer;
    (*ptr)->token = tkn;
    ((char*)buffer)[size-1] = '\0';
    (*ptr)->tokenSize = strlen(tkn);
    (*ptr)->signatureSize = 0;
    (*ptr)->signature = NULL;
    if(used) {
        *used = size;
    }
    return 0;
}

static HRESULT WINAPI x_user_XUserGetTokenAndSignatureUtf16Async( IXUserImpl *iface, XUserHandle user, XUserGetTokenAndSignatureOptions options, LPCWSTR method, LPCWSTR url, SIZE_T count, const XUserGetTokenAndSignatureUtf16HttpHeader *headers, SIZE_T size, const void *buffer, XAsyncBlock *asyncBlock )
{
    abort();
    struct XUserGetTokenAndSignatureContext *context;
    IXThreadingImpl *impl;
    HRESULT hr;

    TRACE( "iface %p, user %p, options %d, method %hs, url %hs, count %llu, headers %p, size %llu, buffer %p, asyncBlock %p\n", iface, user, options, method, url, count, headers, size, buffer, asyncBlock );

    if (!user || !method || !url || !headers || !buffer || !asyncBlock) return E_POINTER;
    if (FAILED( hr = QueryApiImpl( &CLSID_XThreadingImpl, &IID_IXThreadingImpl, (void**)&impl ) )) return hr;
    if (!(context = calloc( 1, sizeof( *context ) )))
    {
        impl->lpVtbl->Release( impl );
        return E_OUTOFMEMORY;
    }

    context->method_utf16 = method;
    context->options = options;
    context->buffer = buffer;
    context->url_utf16 = url;
    context->count = count;
    context->utf16 = TRUE;
    context->size = size;
    context->user = user;
    if (count && !(context->headers_utf16 = calloc( count, sizeof( *headers ) )))
    {
        free( context );
        impl->lpVtbl->Release( impl );
        return E_OUTOFMEMORY;
    }

    for (SIZE_T i = 0; i < count; i++)
        context->headers_utf16[i] = headers[i];

    hr = impl->lpVtbl->XAsyncBegin( impl, asyncBlock, context, x_user_XUserGetTokenAndSignatureUtf16Async, "XUserGetTokenAndSignatureUtf16Async", XUserGetTokenAndSignatureProvider );
    impl->lpVtbl->Release( impl );
    return hr;
}

static HRESULT WINAPI x_user_XUserGetTokenAndSignatureUtf16ResultSize( IXUserImpl *iface, XAsyncBlock *asyncBlock, SIZE_T *size )
{
    abort();
    FIXME( "iface %p, asyncBlock %p, size %p stub!\n", iface, asyncBlock, size );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetTokenAndSignatureUtf16Result( IXUserImpl *iface, XAsyncBlock *asyncBlock, SIZE_T size, PVOID buffer, XUserGetTokenAndSignatureUtf16Data **ptr, SIZE_T *used )
{
    abort();
    FIXME( "iface %p, asyncBlock %p, size %llu, buffer %p, ptr %p, used %p stub!\n", iface, asyncBlock, size, buffer, ptr, used );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserResolveIssueWithUiAsync( IXUserImpl *iface, XUserHandle user, LPCSTR url, XAsyncBlock *asyncBlock )
{
    abort();
    FIXME( "iface %p, user %p, url %s, asyncBlock %p stub!\n", iface, user, url, asyncBlock );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserResolveIssueWithUiResult( IXUserImpl *iface, XAsyncBlock *asyncBlock )
{
    abort();
    FIXME( "iface %p, asyncBlock %p stub!\n", iface, asyncBlock );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserResolveIssueWithUiUtf16Async( IXUserImpl *iface, XUserHandle user, LPCWSTR url, XAsyncBlock *asyncBlock )
{
    abort();
    FIXME( "iface %p, user %p, url %hs, asyncBlock %p stub!\n", iface, user, url, asyncBlock );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserResolveIssueWithUiUtf16Result( IXUserImpl *iface, XAsyncBlock *asyncBlock )
{
    abort();
    FIXME( "iface %p, asyncBlock %p stub!\n", iface, asyncBlock );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserRegisterForChangeEvent( IXUserImpl *iface, XTaskQueueHandle queue, PVOID context, XUserChangeEventCallback *callback, XTaskQueueRegistrationToken *token )
{
    FIXME( "iface %p, context %p, callback %p, token %p stub!\n", iface, context, callback, token );
    // XUserLocalId id;
    // id.value = 1;
    // (*callback)(context, id, XUserChangeEvent_SignedInAgain);
    return 0;
}

static BOOLEAN WINAPI x_user_XUserUnregisterForChangeEvent( IXUserImpl *iface, XTaskQueueRegistrationToken token, BOOLEAN wait )
{
    FIXME( "iface %p, token %p, wait %d stub!\n", iface, &token, wait );
    abort();
    return FALSE;
}

static HRESULT WINAPI x_user_XUserGetSignOutDeferral( IXUserImpl *iface, XUserSignOutDeferralHandle *deferral )
{
    FIXME( "iface %p, deferral %p stub!\n", iface, deferral );
    abort();
    return E_GAMEUSER_DEFERRAL_NOT_AVAILABLE;
}

static void WINAPI x_user_XUserCloseSignOutDeferralHandle( IXUserImpl *iface, XUserSignOutDeferralHandle deferral )
{
    FIXME( "iface %p, deferral %p stub!\n", iface, deferral );
    abort();
}

static HRESULT WINAPI x_user_XUserAddByIdWithUiAsync( IXUserImpl *iface, UINT64 userId, XAsyncBlock *asyncBlock )
{
    FIXME( "iface %p, userId %llu, asyncBlock %p stub!\n", iface, userId, asyncBlock );
    abort();
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserAddByIdWithUiResult( IXUserImpl *iface, XAsyncBlock *asyncBlock, XUserHandle *user )
{
    FIXME( "iface %p, asyncBlock %p, user %p stub!\n", iface, asyncBlock, user );
    abort();
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetMsaTokenSilentlyAsync( IXUserImpl *iface, XUserHandle user, XUserGetMsaTokenSilentlyOptions options, LPCSTR scope, XAsyncBlock *asyncBlock )
{
    FIXME( "iface %p, options %u, scope %s, asyncBlock %p stub!\n", iface, options, scope, asyncBlock );
    abort();
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetMsaTokenSilentlyResult( IXUserImpl *iface, XAsyncBlock *asyncBlock, SIZE_T size, LPSTR token, SIZE_T *used )
{
    FIXME( "iface %p, size %llu, token %p, used %p stub!\n", iface, size, token, used );
    abort();
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetMsaTokenSilentlyResultSize( IXUserImpl *iface, XAsyncBlock *asyncBlock, SIZE_T *size )
{
    FIXME( "iface %p, asyncBlock %p stub!\n", iface, asyncBlock );
    abort();
    return E_NOTIMPL;
}

static BOOLEAN WINAPI x_user_XUserIsStoreUser( IXUserImpl *iface, XUserHandle user )
{
    FIXME( "iface %p, user %p stub!\n", iface, user );
    abort();
    return FALSE;
}

static HRESULT WINAPI x_user_XUserPlatformRemoteConnectSetEventHandlers( IXUserImpl *iface, XTaskQueueHandle queue, XUserPlatformRemoteConnectEventHandlers *handlers )
{
    FIXME( "iface %p, queue %p, handlers %p stub!\n", iface, queue, handlers );
    abort();
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserPlatformRemoteConnectCancelPrompt( IXUserImpl *iface, XUserPlatformOperation operation )
{
    FIXME( "iface %p, operation %p stub!\n", iface, operation );
    abort();
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserPlatformSpopPromptSetEventHandlers( IXUserImpl *iface, XTaskQueueHandle queue, XUserPlatformSpopPromptEventHandler *handler, void *context )
{
    FIXME( "iface %p, queue %p, handler %p, context %p stub!\n", iface, queue, handler, context );
    abort();
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserPlatformSpopPromptComplete( IXUserImpl *iface, XUserPlatformOperation operation, XUserPlatformOperationResult result )
{
    FIXME( "iface %p iface, operation %p, result %d stub!\n", iface, operation, result );
    abort();
    return E_NOTIMPL;
}

static BOOLEAN WINAPI x_user_XUserIsSignOutPresent( IXUserImpl *iface )
{
    FIXME( "iface %p stub!\n", iface );
    abort();
    return FALSE;
}

static HRESULT WINAPI x_user_XUserSignOutAsync( IXUserImpl *iface, XUserHandle user, XAsyncBlock *asyncBlock )
{
    FIXME( "iface %p, user %p, asyncBlock %p stub!\n", iface, user, asyncBlock );
    abort();
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserSignOutResult( IXUserImpl *iface, XAsyncBlock *asyncBlock )
{
    FIXME( "iface %p, asyncBlock %p stub!\n", iface, asyncBlock );
    abort();
    return E_NOTIMPL;
}

static const struct IXUserImplVtbl x_user_vtbl =
{
    /* IUnknown methods */
    x_user_QueryInterface,
    x_user_AddRef,
    x_user_Release,
    /* IXUserBase methods */
    x_user_XUserDuplicateHandle,
    x_user_XUserCloseHandle,
    x_user_XUserCompare,
    x_user_XUserGetMaxUsers,
    x_user_XUserAddAsync,
    x_user_XUserAddResult,
    x_user_XUserGetLocalId,
    x_user_XUserFindUserByLocalId,
    x_user_XUserGetId,
    x_user_XUserFindUserById,
    x_user_XUserGetIsGuest,
    x_user_XUserGetState,
    __PADDING__,
    x_user_XUserGetGamerPictureAsync,
    x_user_XUserGetGamerPictureResultSize,
    x_user_XUserGetGamerPictureResult,
    x_user_XUserGetAgeGroup,
    x_user_XUserCheckPrivilege,
    x_user_XUserResolvePrivilegeWithUiAsync,
    x_user_XUserResolvePrivilegeWithUiResult,
    x_user_XUserGetTokenAndSignatureAsync,
    x_user_XUserGetTokenAndSignatureResultSize,
    x_user_XUserGetTokenAndSignatureResult,
    x_user_XUserGetTokenAndSignatureUtf16Async,
    x_user_XUserGetTokenAndSignatureUtf16ResultSize,
    x_user_XUserGetTokenAndSignatureUtf16Result,
    x_user_XUserResolveIssueWithUiAsync,
    x_user_XUserResolveIssueWithUiResult,
    x_user_XUserResolveIssueWithUiUtf16Async,
    x_user_XUserResolveIssueWithUiUtf16Result,
    x_user_XUserRegisterForChangeEvent,
    x_user_XUserUnregisterForChangeEvent,
    x_user_XUserGetSignOutDeferral,
    x_user_XUserCloseSignOutDeferralHandle,
    /* IXUserAddWithUi methods */
    x_user_XUserAddByIdWithUiAsync,
    x_user_XUserAddByIdWithUiResult,
    /* IXUserMsa methods */
    x_user_XUserGetMsaTokenSilentlyAsync,
    x_user_XUserGetMsaTokenSilentlyResult,
    x_user_XUserGetMsaTokenSilentlyResultSize,
    /* IXUserStore methods */
    x_user_XUserIsStoreUser,
    /* IXUserPlatform methods */
    x_user_XUserPlatformRemoteConnectSetEventHandlers,
    x_user_XUserPlatformRemoteConnectCancelPrompt,
    x_user_XUserPlatformSpopPromptSetEventHandlers,
    x_user_XUserPlatformSpopPromptComplete,
    /* IXUserSignOut methods */
    x_user_XUserIsSignOutPresent,
    x_user_XUserSignOutAsync,
    x_user_XUserSignOutResult
};

static inline struct x_user *impl_from_IXUserGamertag( IXUserGamertag *iface )
{
    return CONTAINING_RECORD( iface, struct x_user, IXUserGamertag_iface );
}

static HRESULT WINAPI x_user_gt_QueryInterface( IXUserGamertag *iface, REFIID iid, void **out )
{
    struct x_user *impl = impl_from_IXUserGamertag( iface );

    TRACE( "iface %p, iid %s, out %p\n", iface, debugstr_guid( iid ), out );

    if (!out) return E_POINTER;

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IXUserBase ) ||
        IsEqualGUID( iid, &IID_IXUserAddWithUi ) ||
        IsEqualGUID( iid, &IID_IXUserMsa ) ||
        IsEqualGUID( iid, &IID_IXUserStore ) ||
        IsEqualGUID( iid, &IID_IXUserPlatform ) ||
        IsEqualGUID( iid, &IID_IXUserSignOut ))
    {
        *out = &impl->IXUserImpl_iface;
        IXUserImpl_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IXUserGamertag ))
    {
        *out = &impl->IXUserGamertag_iface;
        IXUserGamertag_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI x_user_gt_AddRef( IXUserGamertag *iface )
{
    struct x_user *impl = impl_from_IXUserGamertag( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu\n", iface, ref );
    return ref;
}

static ULONG WINAPI x_user_gt_Release( IXUserGamertag *iface )
{
    struct x_user *impl = impl_from_IXUserGamertag( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu\n", iface, ref );
    if (!ref)
    {
        WindowsDeleteString( impl->refresh_token );
        WindowsDeleteString( impl->oauth_token );
        WindowsDeleteString( impl->user_token );
        WindowsDeleteString( impl->xsts_token );
        free( impl );
    }
    return ref;
}

static HRESULT x_user_gt_XUserGetGamertag( IXUserGamertag *iface, XUserHandle user, XUserGamertagComponent component, SIZE_T size, LPSTR gamertag, SIZE_T *used )
{
    FIXME( "iface %p, user %p, component %d, size %llu, gamertag %p, used %p stub!\n", iface, user, component, size, gamertag, used );
    char def[] = "ChristopherHX";
    int len = sizeof(def) < size ? sizeof(def) : size;
    memcpy(gamertag, def, len - 1);
    gamertag[len - 1] = '\0';
    return 0;
}

static const struct IXUserGamertagVtbl x_user_gt_vtbl =
{
    /* IUnknown methods */
    x_user_gt_QueryInterface,
    x_user_gt_AddRef,
    x_user_gt_Release,
    /* IXUserGamertag methods */
    x_user_gt_XUserGetGamertag
};

static struct x_user x_user = {
    {&x_user_vtbl},
    {&x_user_gt_vtbl},
    0,
};

IXUserImpl *x_user_impl = &x_user.IXUserImpl_iface;