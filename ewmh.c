/*
 * ewmh.c - EWMH support functions
 *
 * Copyright © 2007-2009 Julien Danjou <julien@danjou.info>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "ewmh.h"
#include "objects/client.h"
#include "objects/tag.h"
#include "common/atoms.h"
#include "xwindow.h"

#include <sys/types.h>
#include <unistd.h>

#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2

/** Update client EWMH hints.
 * \param L The Lua VM state.
 */
static int
ewmh_client_update_hints(lua_State *L)
{
    client_t *c = luaA_checkudata(L, 1, &client_class);
    xcb_atom_t state[10]; /* number of defined state atoms */
    int i = 0;

    if(c->modal)
        state[i++] = _NET_WM_STATE_MODAL;
    if(c->fullscreen)
        state[i++] = _NET_WM_STATE_FULLSCREEN;
    if(c->maximized_vertical)
        state[i++] = _NET_WM_STATE_MAXIMIZED_VERT;
    if(c->maximized_horizontal)
        state[i++] = _NET_WM_STATE_MAXIMIZED_HORZ;
    if(c->sticky)
        state[i++] = _NET_WM_STATE_STICKY;
    if(c->skip_taskbar)
        state[i++] = _NET_WM_STATE_SKIP_TASKBAR;
    if(c->above)
        state[i++] = _NET_WM_STATE_ABOVE;
    if(c->below)
        state[i++] = _NET_WM_STATE_BELOW;
    if(c->minimized)
        state[i++] = _NET_WM_STATE_HIDDEN;
    if(c->urgent)
        state[i++] = _NET_WM_STATE_DEMANDS_ATTENTION;

    xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
                        c->window, _NET_WM_STATE, XCB_ATOM_ATOM, 32, i, state);

    return 0;
}

static int
ewmh_update_net_active_window(lua_State *L)
{
    xcb_window_t win;

    if(globalconf.focus.client)
        win = globalconf.focus.client->window;
    else
        win = XCB_NONE;

    xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
			globalconf.screen->root,
			_NET_ACTIVE_WINDOW, XCB_ATOM_WINDOW, 32, 1, &win);

    return 0;
}

static int
ewmh_update_net_client_list(lua_State *L)
{
    xcb_window_t *wins = p_alloca(xcb_window_t, globalconf.clients.len);

    int n = 0;
    foreach(client, globalconf.clients)
        wins[n++] = (*client)->window;

    xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
                        globalconf.screen->root,
                        _NET_CLIENT_LIST, XCB_ATOM_WINDOW, 32, n, wins);

    return 0;
}

static int
ewmh_client_update_frame_extents(lua_State *L)
{
    client_t *c = luaA_checkudata(L, 1, &client_class);;
    uint32_t extents[4];

    extents[0] = c->border_width + c->titlebar[CLIENT_TITLEBAR_LEFT].size;
    extents[1] = c->border_width + c->titlebar[CLIENT_TITLEBAR_RIGHT].size;
    extents[2] = c->border_width + c->titlebar[CLIENT_TITLEBAR_TOP].size;
    extents[3] = c->border_width + c->titlebar[CLIENT_TITLEBAR_BOTTOM].size;

    xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
            c->window, _NET_FRAME_EXTENTS, XCB_ATOM_CARDINAL, 32, 4, extents);

    return 0;
}

void
ewmh_init(void)
{
    lua_State *L = globalconf_get_lua_State();
    xcb_window_t father;
    xcb_screen_t *xscreen = globalconf.screen;
    xcb_atom_t atom[] =
    {
        _NET_SUPPORTED,
        _NET_SUPPORTING_WM_CHECK,
        _NET_STARTUP_ID,
        _NET_CLIENT_LIST,
        _NET_CLIENT_LIST_STACKING,
        _NET_NUMBER_OF_DESKTOPS,
        _NET_CURRENT_DESKTOP,
        _NET_DESKTOP_NAMES,
        _NET_ACTIVE_WINDOW,
        _NET_CLOSE_WINDOW,
        _NET_FRAME_EXTENTS,
        _NET_WM_NAME,
        _NET_WM_STRUT_PARTIAL,
        _NET_WM_ICON_NAME,
        _NET_WM_VISIBLE_ICON_NAME,
        _NET_WM_DESKTOP,
        _NET_WM_WINDOW_TYPE,
        _NET_WM_WINDOW_TYPE_DESKTOP,
        _NET_WM_WINDOW_TYPE_DOCK,
        _NET_WM_WINDOW_TYPE_TOOLBAR,
        _NET_WM_WINDOW_TYPE_MENU,
        _NET_WM_WINDOW_TYPE_UTILITY,
        _NET_WM_WINDOW_TYPE_SPLASH,
        _NET_WM_WINDOW_TYPE_DIALOG,
        _NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
        _NET_WM_WINDOW_TYPE_POPUP_MENU,
        _NET_WM_WINDOW_TYPE_TOOLTIP,
        _NET_WM_WINDOW_TYPE_NOTIFICATION,
        _NET_WM_WINDOW_TYPE_COMBO,
        _NET_WM_WINDOW_TYPE_DND,
        _NET_WM_WINDOW_TYPE_NORMAL,
        _NET_WM_ICON,
        _NET_WM_PID,
        _NET_WM_STATE,
        _NET_WM_STATE_STICKY,
        _NET_WM_STATE_SKIP_TASKBAR,
        _NET_WM_STATE_FULLSCREEN,
        _NET_WM_STATE_MAXIMIZED_HORZ,
        _NET_WM_STATE_MAXIMIZED_VERT,
        _NET_WM_STATE_ABOVE,
        _NET_WM_STATE_BELOW,
        _NET_WM_STATE_MODAL,
        _NET_WM_STATE_HIDDEN,
        _NET_WM_STATE_DEMANDS_ATTENTION
    };
    int i;

    xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
                        xscreen->root, _NET_SUPPORTED, XCB_ATOM_ATOM, 32,
                        countof(atom), atom);

    /* create our own window */
    father = xcb_generate_id(globalconf.connection);
    xcb_create_window(globalconf.connection, xscreen->root_depth,
                      father, xscreen->root, -1, -1, 1, 1, 0,
                      XCB_COPY_FROM_PARENT, xscreen->root_visual, 0, NULL);

    xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
                        xscreen->root, _NET_SUPPORTING_WM_CHECK, XCB_ATOM_WINDOW, 32,
                        1, &father);

    xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
                        father, _NET_SUPPORTING_WM_CHECK, XCB_ATOM_WINDOW, 32,
                        1, &father);

    /* set the window manager name */
    xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
                        father, _NET_WM_NAME, UTF8_STRING, 8, 7, "awesome");

    /* Set an instance, just because we can */
    xwindow_set_class_instance(father);

    /* set the window manager PID */
    i = getpid();
    xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
                        father, _NET_WM_PID, XCB_ATOM_CARDINAL, 32, 1, &i);


    luaA_class_connect_signal(L, &client_class, "focus", ewmh_update_net_active_window);
    luaA_class_connect_signal(L, &client_class, "unfocus", ewmh_update_net_active_window);
    luaA_class_connect_signal(L, &client_class, "manage", ewmh_update_net_client_list);
    luaA_class_connect_signal(L, &client_class, "unmanage", ewmh_update_net_client_list);
    luaA_class_connect_signal(L, &client_class, "property::modal" , ewmh_client_update_hints);
    luaA_class_connect_signal(L, &client_class, "property::fullscreen" , ewmh_client_update_hints);
    luaA_class_connect_signal(L, &client_class, "property::maximized_horizontal" , ewmh_client_update_hints);
    luaA_class_connect_signal(L, &client_class, "property::maximized_vertical" , ewmh_client_update_hints);
    luaA_class_connect_signal(L, &client_class, "property::sticky" , ewmh_client_update_hints);
    luaA_class_connect_signal(L, &client_class, "property::skip_taskbar" , ewmh_client_update_hints);
    luaA_class_connect_signal(L, &client_class, "property::above" , ewmh_client_update_hints);
    luaA_class_connect_signal(L, &client_class, "property::below" , ewmh_client_update_hints);
    luaA_class_connect_signal(L, &client_class, "property::minimized" , ewmh_client_update_hints);
    luaA_class_connect_signal(L, &client_class, "property::urgent" , ewmh_client_update_hints);
    luaA_class_connect_signal(L, &client_class, "property::titlebar_top" , ewmh_client_update_frame_extents);
    luaA_class_connect_signal(L, &client_class, "property::titlebar_bottom" , ewmh_client_update_frame_extents);
    luaA_class_connect_signal(L, &client_class, "property::titlebar_right" , ewmh_client_update_frame_extents);
    luaA_class_connect_signal(L, &client_class, "property::titlebar_left" , ewmh_client_update_frame_extents);
    luaA_class_connect_signal(L, &client_class, "property::border_width" , ewmh_client_update_frame_extents);
    luaA_class_connect_signal(L, &client_class, "manage", ewmh_client_update_frame_extents);
}

/** Set the client list in stacking order, bottom to top.
 */
void
ewmh_update_net_client_list_stacking(void)
{
    int n = 0;
    xcb_window_t *wins = p_alloca(xcb_window_t, globalconf.stack.len);

    foreach(client, globalconf.stack)
        wins[n++] = (*client)->window;

    xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
			globalconf.screen->root,
			_NET_CLIENT_LIST_STACKING, XCB_ATOM_WINDOW, 32, n, wins);
}

void
ewmh_update_net_numbers_of_desktop(void)
{
    uint32_t count = globalconf.tags.len;

    xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
			globalconf.screen->root,
			_NET_NUMBER_OF_DESKTOPS, XCB_ATOM_CARDINAL, 32, 1, &count);
}

void
ewmh_update_net_current_desktop(void)
{
    uint32_t idx = tags_get_first_selected_index();

    xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
                        globalconf.screen->root,
                        _NET_CURRENT_DESKTOP, XCB_ATOM_CARDINAL, 32, 1, &idx);
}

void
ewmh_update_net_desktop_names(void)
{
    buffer_t buf;

    buffer_inita(&buf, BUFSIZ);

    foreach(tag, globalconf.tags)
    {
        buffer_adds(&buf, tag_get_name(*tag));
        buffer_addc(&buf, '\0');
    }

    xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
			globalconf.screen->root,
			_NET_DESKTOP_NAMES, UTF8_STRING, 8, buf.len, buf.s);
    buffer_wipe(&buf);
}

static void
ewmh_process_state_atom(client_t *c, xcb_atom_t state, int set)
{
    lua_State *L = globalconf_get_lua_State();
    luaA_object_push(L, c);

    if(state == _NET_WM_STATE_STICKY)
    {
        if(set == _NET_WM_STATE_REMOVE)
            client_set_sticky(L, -1, false);
        else if(set == _NET_WM_STATE_ADD)
            client_set_sticky(L, -1, true);
        else if(set == _NET_WM_STATE_TOGGLE)
            client_set_sticky(L, -1, !c->sticky);
    }
    else if(state == _NET_WM_STATE_SKIP_TASKBAR)
    {
        if(set == _NET_WM_STATE_REMOVE)
            client_set_skip_taskbar(L, -1, false);
        else if(set == _NET_WM_STATE_ADD)
            client_set_skip_taskbar(L, -1, true);
        else if(set == _NET_WM_STATE_TOGGLE)
            client_set_skip_taskbar(L, -1, !c->skip_taskbar);
    }
    else if(state == _NET_WM_STATE_FULLSCREEN)
    {
        if(set == _NET_WM_STATE_REMOVE)
            client_set_fullscreen(L, -1, false);
        else if(set == _NET_WM_STATE_ADD)
            client_set_fullscreen(L, -1, true);
        else if(set == _NET_WM_STATE_TOGGLE)
            client_set_fullscreen(L, -1, !c->fullscreen);
    }
    else if(state == _NET_WM_STATE_MAXIMIZED_HORZ)
    {
        if(set == _NET_WM_STATE_REMOVE)
            client_set_maximized_horizontal(L, -1, false);
        else if(set == _NET_WM_STATE_ADD)
            client_set_maximized_horizontal(L, -1, true);
        else if(set == _NET_WM_STATE_TOGGLE)
            client_set_maximized_horizontal(L, -1, !c->maximized_horizontal);
    }
    else if(state == _NET_WM_STATE_MAXIMIZED_VERT)
    {
        if(set == _NET_WM_STATE_REMOVE)
            client_set_maximized_vertical(L, -1, false);
        else if(set == _NET_WM_STATE_ADD)
            client_set_maximized_vertical(L, -1, true);
        else if(set == _NET_WM_STATE_TOGGLE)
            client_set_maximized_vertical(L, -1, !c->maximized_vertical);
    }
    else if(state == _NET_WM_STATE_ABOVE)
    {
        if(set == _NET_WM_STATE_REMOVE)
            client_set_above(L, -1, false);
        else if(set == _NET_WM_STATE_ADD)
            client_set_above(L, -1, true);
        else if(set == _NET_WM_STATE_TOGGLE)
            client_set_above(L, -1, !c->above);
    }
    else if(state == _NET_WM_STATE_BELOW)
    {
        if(set == _NET_WM_STATE_REMOVE)
            client_set_below(L, -1, false);
        else if(set == _NET_WM_STATE_ADD)
            client_set_below(L, -1, true);
        else if(set == _NET_WM_STATE_TOGGLE)
            client_set_below(L, -1, !c->below);
    }
    else if(state == _NET_WM_STATE_MODAL)
    {
        if(set == _NET_WM_STATE_REMOVE)
            client_set_modal(L, -1, false);
        else if(set == _NET_WM_STATE_ADD)
            client_set_modal(L, -1, true);
        else if(set == _NET_WM_STATE_TOGGLE)
            client_set_modal(L, -1, !c->modal);
    }
    else if(state == _NET_WM_STATE_HIDDEN)
    {
        if(set == _NET_WM_STATE_REMOVE)
            client_set_minimized(L, -1, false);
        else if(set == _NET_WM_STATE_ADD)
            client_set_minimized(L, -1, true);
        else if(set == _NET_WM_STATE_TOGGLE)
            client_set_minimized(L, -1, !c->minimized);
    }
    else if(state == _NET_WM_STATE_DEMANDS_ATTENTION)
    {
        if(set == _NET_WM_STATE_REMOVE) {
            lua_pushboolean(L, false);
            luaA_object_emit_signal(L, -2, "request::urgent", 1);
        }
        else if(set == _NET_WM_STATE_ADD) {
            lua_pushboolean(L, true);
            luaA_object_emit_signal(L, -2, "request::urgent", 1);
        }
        else if(set == _NET_WM_STATE_TOGGLE) {
            lua_pushboolean(L, !c->urgent);
            luaA_object_emit_signal(L, -2, "request::urgent", 1);
        }
    }

    lua_pop(L, 1);
}

static void
ewmh_process_desktop(client_t *c, uint32_t desktop)
{
    lua_State *L = globalconf_get_lua_State();
    int idx = desktop;
    if(desktop == 0xffffffff)
    {
        luaA_object_push(L, c);
        lua_pushnil(L);
        luaA_object_emit_signal(L, -2, "request::tag", 1);
        /* Pop the client, arguments are already popped */
        lua_pop(L, 1);
    }
    else if (idx >= 0 && idx < globalconf.tags.len)
    {
        luaA_object_push(L, c);
        luaA_object_push(L, globalconf.tags.tab[idx]);
        luaA_object_emit_signal(L, -2, "request::tag", 1);
        /* Pop the client, arguments are already popped */
        lua_pop(L, 1);
    }
}

int
ewmh_process_client_message(xcb_client_message_event_t *ev)
{
    client_t *c;

    if(ev->type == _NET_CURRENT_DESKTOP)
    {
        int idx = ev->data.data32[0];
        if (idx >= 0 && idx < globalconf.tags.len)
        {
            lua_State *L = globalconf_get_lua_State();
            luaA_object_push(L, globalconf.tags.tab[idx]);
            luaA_object_emit_signal(L, -1, "request::select", 0);
            lua_pop(L, 1);
        }
    }
    else if(ev->type == _NET_CLOSE_WINDOW)
    {
        if((c = client_getbywin(ev->window)))
           client_kill(c);
    }
    else if(ev->type == _NET_WM_DESKTOP)
    {
        if((c = client_getbywin(ev->window)))
        {
            ewmh_process_desktop(c, ev->data.data32[0]);
        }
    }
    else if(ev->type == _NET_WM_STATE)
    {
        if((c = client_getbywin(ev->window)))
        {
            ewmh_process_state_atom(c, (xcb_atom_t) ev->data.data32[1], ev->data.data32[0]);
            if(ev->data.data32[2])
                ewmh_process_state_atom(c, (xcb_atom_t) ev->data.data32[2],
                                        ev->data.data32[0]);
        }
    }
    else if(ev->type == _NET_ACTIVE_WINDOW)
    {
        if((c = client_getbywin(ev->window))) {
            lua_State *L = globalconf_get_lua_State();
            luaA_object_push(L, c);
            lua_pushstring(L, "ewmh");

            /* Create table argument with raise=true. */
            lua_newtable(L);
            lua_pushstring(L, "raise");
            lua_pushboolean(L, true);
            lua_settable(L, -3);

            luaA_object_emit_signal(L, -3, "request::activate", 2);
            lua_pop(L, 1);
        }
    }

    return 0;
}

/** Update the client active desktop.
 * This is "wrong" since it can be on several tags, but EWMH has a strict view
 * of desktop system so just take the first tag.
 * \param c The client.
 */
void
ewmh_client_update_desktop(client_t *c)
{
    int i;

    for(i = 0; i < globalconf.tags.len; i++)
        if(is_client_tagged(c, globalconf.tags.tab[i]))
        {
            xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
                                c->window, _NET_WM_DESKTOP, XCB_ATOM_CARDINAL, 32, 1, &i);
            return;
        }
    /* It doesn't have any tags, remove the property */
    xcb_delete_property(globalconf.connection, c->window, _NET_WM_DESKTOP);
}

/** Update the client struts.
 * \param window The window to update the struts for.
 * \param strut The strut type to update the window with.
 */
void
ewmh_update_strut(xcb_window_t window, strut_t *strut)
{
    if(window)
    {
        const uint32_t state[] =
        {
            strut->left,
            strut->right,
            strut->top,
            strut->bottom,
            strut->left_start_y,
            strut->left_end_y,
            strut->right_start_y,
            strut->right_end_y,
            strut->top_start_x,
            strut->top_end_x,
            strut->bottom_start_x,
            strut->bottom_end_x
        };

        xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
                            window, _NET_WM_STRUT_PARTIAL, XCB_ATOM_CARDINAL, 32, countof(state), state);
    }
}

/** Update the window type.
 * \param window The window to update.
 * \param type The new type to set.
 */
void
ewmh_update_window_type(xcb_window_t window, uint32_t type)
{
    xcb_change_property(globalconf.connection, XCB_PROP_MODE_REPLACE,
                        window, _NET_WM_WINDOW_TYPE, XCB_ATOM_ATOM, 32, 1, &type);
}

void
ewmh_client_check_hints(client_t *c)
{
    xcb_atom_t *state;
    void *data = NULL;
    xcb_get_property_cookie_t c0, c1, c2;
    xcb_get_property_reply_t *reply;

    /* Send the GetProperty requests which will be processed later */
    c0 = xcb_get_property_unchecked(globalconf.connection, false, c->window,
                                    _NET_WM_DESKTOP, XCB_GET_PROPERTY_TYPE_ANY, 0, 1);

    c1 = xcb_get_property_unchecked(globalconf.connection, false, c->window,
                                    _NET_WM_STATE, XCB_ATOM_ATOM, 0, UINT32_MAX);

    c2 = xcb_get_property_unchecked(globalconf.connection, false, c->window,
                                    _NET_WM_WINDOW_TYPE, XCB_ATOM_ATOM, 0, UINT32_MAX);

    reply = xcb_get_property_reply(globalconf.connection, c0, NULL);
    if(reply && reply->value_len && (data = xcb_get_property_value(reply)))
    {
        ewmh_process_desktop(c, *(uint32_t *) data);
    }

    p_delete(&reply);

    reply = xcb_get_property_reply(globalconf.connection, c1, NULL);
    if(reply && (data = xcb_get_property_value(reply)))
    {
        state = (xcb_atom_t *) data;
        for(int i = 0; i < xcb_get_property_value_length(reply) / ssizeof(xcb_atom_t); i++)
            ewmh_process_state_atom(c, state[i], _NET_WM_STATE_ADD);
    }

    p_delete(&reply);

    reply = xcb_get_property_reply(globalconf.connection, c2, NULL);
    if(reply && (data = xcb_get_property_value(reply)))
    {
        state = (xcb_atom_t *) data;
        for(int i = 0; i < xcb_get_property_value_length(reply) / ssizeof(xcb_atom_t); i++)
            if(state[i] == _NET_WM_WINDOW_TYPE_DESKTOP)
                c->type = MAX(c->type, WINDOW_TYPE_DESKTOP);
            else if(state[i] == _NET_WM_WINDOW_TYPE_DIALOG)
                c->type = MAX(c->type, WINDOW_TYPE_DIALOG);
            else if(state[i] == _NET_WM_WINDOW_TYPE_SPLASH)
                c->type = MAX(c->type, WINDOW_TYPE_SPLASH);
            else if(state[i] == _NET_WM_WINDOW_TYPE_DOCK)
                c->type = MAX(c->type, WINDOW_TYPE_DOCK);
            else if(state[i] == _NET_WM_WINDOW_TYPE_MENU)
                c->type = MAX(c->type, WINDOW_TYPE_MENU);
            else if(state[i] == _NET_WM_WINDOW_TYPE_TOOLBAR)
                c->type = MAX(c->type, WINDOW_TYPE_TOOLBAR);
            else if(state[i] == _NET_WM_WINDOW_TYPE_UTILITY)
                c->type = MAX(c->type, WINDOW_TYPE_UTILITY);
    }

    p_delete(&reply);
}

/** Process the WM strut of a client.
 * \param c The client.
 */
void
ewmh_process_client_strut(client_t *c)
{
    void *data;
    xcb_get_property_reply_t *strut_r;

    xcb_get_property_cookie_t strut_q = xcb_get_property_unchecked(globalconf.connection, false, c->window,
                                                                   _NET_WM_STRUT_PARTIAL, XCB_ATOM_CARDINAL, 0, 12);
    strut_r = xcb_get_property_reply(globalconf.connection, strut_q, NULL);

    if(strut_r
       && strut_r->value_len
       && (data = xcb_get_property_value(strut_r)))
    {
        uint32_t *strut = data;

        if(c->strut.left != strut[0]
           || c->strut.right != strut[1]
           || c->strut.top != strut[2]
           || c->strut.bottom != strut[3]
           || c->strut.left_start_y != strut[4]
           || c->strut.left_end_y != strut[5]
           || c->strut.right_start_y != strut[6]
           || c->strut.right_end_y != strut[7]
           || c->strut.top_start_x != strut[8]
           || c->strut.top_end_x != strut[9]
           || c->strut.bottom_start_x != strut[10]
           || c->strut.bottom_end_x != strut[11])
        {
            c->strut.left = strut[0];
            c->strut.right = strut[1];
            c->strut.top = strut[2];
            c->strut.bottom = strut[3];
            c->strut.left_start_y = strut[4];
            c->strut.left_end_y = strut[5];
            c->strut.right_start_y = strut[6];
            c->strut.right_end_y = strut[7];
            c->strut.top_start_x = strut[8];
            c->strut.top_end_x = strut[9];
            c->strut.bottom_start_x = strut[10];
            c->strut.bottom_end_x = strut[11];

            lua_State *L = globalconf_get_lua_State();
            luaA_object_push(L, c);
            luaA_object_emit_signal(L, -1, "property::struts", 0);
            lua_pop(L, 1);
        }
    }

    p_delete(&strut_r);
}

/** Send request to get NET_WM_ICON (EWMH)
 * \param w The window.
 * \return The cookie associated with the request.
 */
xcb_get_property_cookie_t
ewmh_window_icon_get_unchecked(xcb_window_t w)
{
  return xcb_get_property_unchecked(globalconf.connection, false, w,
                                    _NET_WM_ICON, XCB_ATOM_CARDINAL, 0, UINT32_MAX);
}

static cairo_surface_t *
ewmh_window_icon_from_reply(xcb_get_property_reply_t *r, uint32_t preferred_size)
{
    uint32_t *data, *end, *found_data = 0;
    uint32_t found_size = 0;

    if(!r || r->type != XCB_ATOM_CARDINAL || r->format != 32 || r->length < 2)
        return 0;

    data = (uint32_t *) xcb_get_property_value(r);
    if (!data) return 0;

    end = data + r->length;

    /* Goes over the icon data and picks the icon that best matches the size preference.
     * In case the size match is not exact, picks the closest bigger size if present,
     * closest smaller size otherwise.
     */
    while (data + 1 < end) {
        /* check whether the data size specified by width and height fits into the array we got */
        uint64_t data_size = (uint64_t) data[0] * data[1];
        if (data_size > (uint64_t) (end - data - 2)) break;

        /* use the greater of the two dimensions to match against the preferred size */
        uint32_t size = MAX(data[0], data[1]);

        /* pick the icon if it's a better match than the one we already have */
        bool found_icon_too_small = found_size < preferred_size;
        bool found_icon_too_large = found_size > preferred_size;
        bool icon_empty = data[0] == 0 || data[1] == 0;
        bool better_because_bigger =  found_icon_too_small && size > found_size;
        bool better_because_smaller = found_icon_too_large &&
            size >= preferred_size && size < found_size;
        if (!icon_empty && (better_because_bigger || better_because_smaller || found_size == 0))
        {
            found_data = data;
            found_size = size;
        }

        data += data_size + 2;
    }

    if (!found_data) return 0;

    return draw_surface_from_data(found_data[0], found_data[1], found_data + 2);
}

/** Get NET_WM_ICON.
 * \param cookie The cookie.
 * \return The number of elements on stack.
 */
cairo_surface_t *
ewmh_window_icon_get_reply(xcb_get_property_cookie_t cookie, uint32_t preferred_size)
{
    xcb_get_property_reply_t *r = xcb_get_property_reply(globalconf.connection, cookie, NULL);
    cairo_surface_t *surface = ewmh_window_icon_from_reply(r, preferred_size);
    p_delete(&r);
    return surface;
}

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:textwidth=80
