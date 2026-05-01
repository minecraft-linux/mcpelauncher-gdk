#include "XGameLaunch.h"

WINE_DEFAULT_DEBUG_CHANNEL(gdkc);

static const struct IXGameLaunchImplVtbl x_gamelaunch_vtbl;

static inline struct x_game_launch *impl_from_IXGameLaunchImpl( IXGameLaunchImpl *iface )
{
    return CONTAINING_RECORD( iface, struct x_game_launch, IXGameLaunch_iface );
}

static HRESULT WINAPI x_game_launch_QueryInterface( IXGameLaunchImpl *iface, REFIID iid, void **out )
{
    struct x_game_launch *impl = impl_from_IXGameLaunchImpl( iface );

    TRACE( "iface %p, iid %s, out %p\n", iface, debugstr_guid( iid ), out );

    if (!out) return E_POINTER;

    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IXGameLaunch ))
    {
        *out = &impl->IXGameLaunch_iface;
        IXGameLaunchImpl_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI x_game_launch_AddRef( IXGameLaunchImpl *iface )
{
    struct x_game_launch *impl = impl_from_IXGameLaunchImpl( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu\n", iface, ref );
    return ref;
}

static ULONG WINAPI x_game_launch_Release( IXGameLaunchImpl *iface )
{
    struct x_game_launch *impl = impl_from_IXGameLaunchImpl( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu\n", iface, ref );
    if (!ref)
    {
        free( impl );
    }
    return ref;
}

/*** IXGameLaunch methods ***/
HRESULT XGameGetXboxTitleId(IXGameLaunchImpl *This, uint32_t *titleId) {
    *titleId = 0x35760C07;
    return 0;
}

HRESULT STUB1(IXGameLaunchImpl *This) {
    return E_NOTIMPL;
}


HRESULT STUB2(IXGameLaunchImpl *This) {
    return E_NOTIMPL;
}


static const struct IXGameLaunchImplVtbl x_gamelaunch_vtbl =
{
    /* IUnknown methods */
    x_game_launch_QueryInterface,
    x_game_launch_AddRef,
    x_game_launch_Release,
    /*** IXGameLaunch methods ***/
    XGameGetXboxTitleId,
    STUB1,
    STUB2
};

static struct x_game_launch x_game_launch = {
    {&x_gamelaunch_vtbl},
    0,
};

IXGameLaunchImpl *x_game_launch_impl = &x_game_launch.IXGameLaunch_iface;